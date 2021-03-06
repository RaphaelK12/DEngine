#include "Vk.hpp"
#include "../Assert.hpp"
#include "Init.hpp"

#include "DEngine/FixedWidthTypes.hpp"
#include "DEngine/Containers/Span.hpp"
#include "DEngine/Containers/StaticVector.hpp"
// For file IO
#include "DEngine/Application.hpp"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_vulkan.h"

#include <string>

//vk::DispatchLoaderDynamic vk::defaultDispatchLoaderDynamic;

namespace DEngine::Gfx::Vk
{
	bool InitializeBackend(Data& gfxData, InitInfo const& initInfo, void*& apiDataBuffer);

	namespace Init
	{
		void Test(APIData& apiData);
	}
}

DEngine::Gfx::Vk::APIData::APIData()
{
}

DEngine::Gfx::Vk::APIData::~APIData()
{
	APIData& apiData = *this;

	apiData.globUtils.device.WaitIdle();
}

void DEngine::Gfx::Vk::APIData::NewViewport(uSize& viewportID, void*& imguiTexID)
{
	APIData& apiData = *this;

	apiData.viewportManager.NewViewport(viewportID, imguiTexID);
}

void DEngine::Gfx::Vk::APIData::DeleteViewport(uSize id)
{
	vk::Result vkResult{};
	APIData& apiData = *this;

	apiData.viewportManager.DeleteViewport(id);
}

DEngine::Gfx::Vk::GlobUtils::GlobUtils()
{
}

DEngine::u8 DEngine::Gfx::Vk::GlobUtils::CurrentResourceSetIndex_Async()
{
	u8 index = static_cast<u8>(-1);
	{
		std::lock_guard _{ currentResourceSetIndex_Lock };
		index = currentResourceSetIndex_Var;
	}
	return index;
}

void DEngine::Gfx::Vk::GlobUtils::SetCurrentResourceSetIndex(u8 index)
{
	std::lock_guard _{ currentResourceSetIndex_Lock };
	currentResourceSetIndex_Var = index;
}

bool DEngine::Gfx::Vk::InitializeBackend(Data& gfxData, InitInfo const& initInfo, void*& apiDataBuffer)
{
	apiDataBuffer = new APIData;
	APIData& apiData = *static_cast<APIData*>(apiDataBuffer);
	GlobUtils& globUtils = apiData.globUtils;

	vk::Result vkResult{};
	bool boolResult = false;

	apiData.logger = initInfo.optional_iLog;
	apiData.globUtils.logger = initInfo.optional_iLog;
	apiData.wsiInterface = initInfo.iWsi;
	apiData.test_textureAssetInterface = initInfo.texAssetInterface;

	globUtils.resourceSetCount = 2;
	apiData.currentInFlightFrame = 0;
	globUtils.useEditorPipeline = true;

	// Make the VkInstance
	PFN_vkGetInstanceProcAddr instanceProcAddr = Vk::loadInstanceProcAddressPFN();
	BaseDispatch baseDispatch = BaseDispatch::Build(instanceProcAddr);
	Init::CreateVkInstance_Return createVkInstanceResult = Init::CreateVkInstance(
		initInfo.requiredVkInstanceExtensions, 
		true, 
		baseDispatch, 
		apiData.logger);
	InstanceDispatch instance = InstanceDispatch::Build(createVkInstanceResult.instanceHandle, instanceProcAddr);
	globUtils.instance = instance;

	// Enable DebugUtils functionality.
	// If we enabled debug utils, we build the debug utils dispatch table and the debug utils messenger.
	if (createVkInstanceResult.debugUtilsEnabled)
	{
		globUtils.debugUtils = DebugUtilsDispatch::Build(globUtils.instance.handle, instanceProcAddr);
		globUtils.debugMessenger = Init::CreateLayerMessenger(
			globUtils.instance.handle, 
			globUtils.DebugUtilsPtr(), 
			initInfo.optional_iLog);
	}

	// TODO: I don't think I like this code
	// Create the VkSurface using the callback
	vk::SurfaceKHR surface{};
	vk::Result surfaceCreateResult = (vk::Result)apiData.wsiInterface->CreateVkSurface(
		(u64)(VkInstance)instance.handle, 
		nullptr, 
		*reinterpret_cast<u64*>(&surface));
	if (surfaceCreateResult != vk::Result::eSuccess)
		throw std::runtime_error("Unable to create VkSurfaceKHR object during initialization.");

	// Pick our phys device and load the info for it
	apiData.globUtils.physDevice = Init::LoadPhysDevice(instance, surface);

	// Build the surface info now that we have our physical device.
	apiData.surface = Init::BuildSurfaceInfo(instance, globUtils.physDevice.handle, surface, apiData.logger);

	// Build the settings we will use to build the swapchain.
	SwapchainSettings swapchainSettings = Init::BuildSwapchainSettings(
		instance,
		globUtils.physDevice.handle,
		apiData.surface,
		apiData.surface.capabilities.currentExtent.width,
		apiData.surface.capabilities.currentExtent.height,
		apiData.logger);

	PFN_vkGetDeviceProcAddr deviceProcAddr = (PFN_vkGetDeviceProcAddr)instanceProcAddr((VkInstance)instance.handle, "vkGetDeviceProcAddr");

	// CreateJob the device and the dispatch table for it.
	vk::Device deviceHandle = Init::CreateDevice(instance, globUtils.physDevice);
	globUtils.device.copy(DeviceDispatch::Build(deviceHandle, deviceProcAddr));
	globUtils.device.m_queueDataPtr = &globUtils.queues;

	boolResult = ViewportManager::Init(
		apiData.viewportManager,
		globUtils.device,
		globUtils.physDevice.properties.limits.minUniformBufferOffsetAlignment,
		globUtils.DebugUtilsPtr());
	if (!boolResult)
		throw std::runtime_error("DEngine - Vulkan: Failed to initialize ViewportManager.");

	boolResult = DeletionQueue::Init(
		globUtils.deletionQueue,
		&globUtils,
		globUtils.resourceSetCount);
	if (!boolResult)
		throw std::runtime_error("DEngine - Vulkan: Failed to initialize DeletionQueue");

	QueueData::Init(
		globUtils.queues,
		globUtils.device, 
		globUtils.physDevice.queueIndices, 
		globUtils.DebugUtilsPtr());

	TextureManager::Init(
		apiData.textureManager,
		globUtils.device,
		globUtils.queues,
		globUtils.DebugUtilsPtr());

	// Init VMA
	apiData.vma_trackingData.deviceHandle = globUtils.device.handle;
	apiData.vma_trackingData.debugUtils = globUtils.DebugUtilsPtr();
	vk::ResultValue<VmaAllocator> vmaResult = Init::InitializeVMA(
		globUtils.instance,
		globUtils.physDevice.handle,
		globUtils.device,
		&apiData.vma_trackingData);
	if (vmaResult.result != vk::Result::eSuccess)
		throw std::runtime_error("DEngine - Vulkan: Failed to initialize VMA.");
	else
		globUtils.vma = vmaResult.value;

	boolResult = ObjectDataManager::Init(
		apiData.objectDataManager,
		globUtils.physDevice.properties.limits.minUniformBufferOffsetAlignment,
		globUtils.resourceSetCount,
		globUtils.vma,
		globUtils.device,
		globUtils.DebugUtilsPtr());
	if (!boolResult)
		throw std::runtime_error("DEngine - Vulkan: Failed to initialize ObjectDataManager.");


	// Build our swapchain on our device
	apiData.swapchain = Init::CreateSwapchain(
		apiData.globUtils.device, 
		apiData.globUtils.queues,
		apiData.globUtils.deletionQueue,
		swapchainSettings,
		apiData.globUtils.DebugUtilsPtr());


	// Create our main fences
	apiData.mainFences = Init::CreateMainFences(
		apiData.globUtils.device,
		apiData.globUtils.resourceSetCount,
		apiData.globUtils.DebugUtilsPtr());


	apiData.globUtils.guiRenderPass = Init::CreateGuiRenderPass(
			apiData.globUtils.device,
			apiData.swapchain.surfaceFormat.format,
			globUtils.DebugUtilsPtr());
	// Create the resources for rendering GUI
	apiData.guiData = Init::CreateGUIData(
		apiData.globUtils.device,
		apiData.globUtils.vma,
		apiData.globUtils.deletionQueue,
		apiData.globUtils.queues,
		globUtils.guiRenderPass,
		apiData.swapchain.surfaceFormat.format,
		apiData.swapchain.extents,
		apiData.globUtils.resourceSetCount,
		apiData.globUtils.DebugUtilsPtr());

	// Record the copy-image command cmdBuffers that go from render-target to swapchain
	Init::RecordSwapchainCmdBuffers(
		apiData.globUtils.device, 
		apiData.swapchain, 
		apiData.guiData.renderTarget.img);

	// Initialize ImGui stuff
	Init::InitializeImGui(
		apiData, apiData.globUtils.device, 
		instanceProcAddr, 
		apiData.globUtils.DebugUtilsPtr());

	// Create the main render stuff
	apiData.globUtils.gfxRenderPass = Init::BuildMainGfxRenderPass(
		apiData.globUtils.device,
		true, 
		apiData.globUtils.DebugUtilsPtr());


	





	Init::Test(apiData);
	

	return true;
}

void DEngine::Gfx::Vk::Init::Test(APIData& apiData)
{
	vk::Result vkResult{};

	Std::Array<vk::DescriptorSetLayout, 3> layouts{ 
		apiData.viewportManager.cameraDescrLayout, 
		apiData.objectDataManager.descrSetLayout,
		apiData.textureManager.descrSetLayout };

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.setLayoutCount = 3;
	pipelineLayoutInfo.pSetLayouts = layouts.Data();
	apiData.testPipelineLayout = apiData.globUtils.device.createPipelineLayout(pipelineLayoutInfo);

	App::FileInputStream vertFile{ "data/vert.spv" };
	if (!vertFile.IsOpen())
		throw std::runtime_error("Could not open vertex shader file");
	vertFile.Seek(0, App::FileInputStream::SeekOrigin::End);
	u64 vertFileLength = vertFile.Tell().Value();
	vertFile.Seek(0, App::FileInputStream::SeekOrigin::Start);
	std::vector<char> vertCode((uSize)vertFileLength);
	vertFile.Read(vertCode.data(), vertFileLength);
	
	vk::ShaderModuleCreateInfo vertModCreateInfo{};
	vertModCreateInfo.codeSize = vertCode.size();
	vertModCreateInfo.pCode = reinterpret_cast<const u32*>(vertCode.data());
	vk::ShaderModule vertModule = apiData.globUtils.device.createShaderModule(vertModCreateInfo);
	vk::PipelineShaderStageCreateInfo vertStageInfo{};
	vertStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	vertStageInfo.module = vertModule;
	vertStageInfo.pName = "main";

	App::FileInputStream fragFile{ "data/frag.spv" };
	if (!fragFile.IsOpen())
		throw std::runtime_error("Could not open fragment shader file");
	fragFile.Seek(0, App::FileInputStream::SeekOrigin::End);
	u64 fragFileLength = fragFile.Tell().Value();
	fragFile.Seek(0, App::FileInputStream::SeekOrigin::Start);
	std::vector<char> fragCode((uSize)fragFileLength);
	fragFile.Read(fragCode.data(), fragFileLength);

	vk::ShaderModuleCreateInfo fragModInfo{};
	fragModInfo.codeSize = fragCode.size();
	fragModInfo.pCode = reinterpret_cast<const u32*>(fragCode.data());
	vk::ShaderModule fragModule = apiData.globUtils.device.createShaderModule(fragModInfo);
	vk::PipelineShaderStageCreateInfo fragStageInfo{};
	fragStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	fragStageInfo.module = fragModule;
	fragStageInfo.pName = "main";

	Std::Array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = { vertStageInfo, fragStageInfo };

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.topology = vk::PrimitiveTopology::eTriangleStrip;

	vk::Viewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (f32)0.f;
	viewport.height = (f32)0.f;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vk::Rect2D scissor{};
	scissor.offset = vk::Offset2D{ 0, 0 };
	scissor.extent = vk::Extent2D{ 8192, 8192 };
	vk::PipelineViewportStateCreateInfo viewportState{};
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	vk::PipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.lineWidth = 1.f;
	rasterizer.polygonMode = vk::PolygonMode::eFill;
	rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
	rasterizer.rasterizerDiscardEnable = 0;
	rasterizer.cullMode = vk::CullModeFlagBits::eNone;

	vk::PipelineDepthStencilStateCreateInfo depthStencilInfo{};
	depthStencilInfo.depthTestEnable = 0;
	depthStencilInfo.depthCompareOp = vk::CompareOp::eLess;
	depthStencilInfo.stencilTestEnable = 0;
	depthStencilInfo.depthWriteEnable = 0;
	depthStencilInfo.minDepthBounds = 0.f;
	depthStencilInfo.maxDepthBounds = 1.f;

	vk::PipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sampleShadingEnable = 0;
	multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

	vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

	vk::PipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	vk::DynamicState temp = vk::DynamicState::eViewport;
	vk::PipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.dynamicStateCount = 1;
	dynamicState.pDynamicStates = &temp;

	vk::GraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.layout = apiData.testPipelineLayout;
	pipelineInfo.renderPass = apiData.globUtils.gfxRenderPass;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.stageCount = (u32)shaderStages.Size();
	pipelineInfo.pStages = shaderStages.Data();

	vkResult = apiData.globUtils.device.createGraphicsPipelines(
		vk::PipelineCache(),
		{ 1, &pipelineInfo },
		nullptr,
		&apiData.testPipeline);
	if (vkResult != vk::Result::eSuccess)
		throw std::runtime_error("Unable to make graphics pipeline.");

	apiData.globUtils.device.Destroy(vertModule);
	apiData.globUtils.device.Destroy(fragModule);
	
}