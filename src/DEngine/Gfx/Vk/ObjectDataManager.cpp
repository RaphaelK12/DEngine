#include "ObjectDataManager.hpp"
#include "GlobUtils.hpp"

#include "DEngine/Math/Common.hpp"

#include "../Assert.hpp"

#include <string>

void DEngine::Gfx::Vk::ObjectDataManager::HandleResizeEvent(
	ObjectDataManager& manager,
	GlobUtils const& globUtils,
	uSize dataCount)
{
	vk::Result vkResult{};

	if (dataCount <= manager.capacity)
		return;

	// Queue previous stuff for deletion
	globUtils.deletionQueue.Destroy(manager.descrPool);
	globUtils.deletionQueue.Destroy(manager.vmaAlloc, manager.buffer);

	manager.capacity *= 2;

	// Allocate the buffer
	vk::BufferCreateInfo buffInfo{};
	buffInfo.sharingMode = vk::SharingMode::eExclusive;
	buffInfo.size = manager.elementSize * manager.capacity * globUtils.resourceSetCount;
	buffInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer;
	VmaAllocationCreateInfo vmaAllocInfo{};
	vmaAllocInfo.flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_MAPPED_BIT;
	vmaAllocInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU;
	VmaAllocationInfo vmaAllocResultInfo{};
	vkResult = (vk::Result)vmaCreateBuffer(
		globUtils.vma,
		(VkBufferCreateInfo const*)&buffInfo,
		&vmaAllocInfo,
		(VkBuffer*)&manager.buffer,
		&manager.vmaAlloc,
		&vmaAllocResultInfo);
	if (vkResult != vk::Result::eSuccess)
		throw std::runtime_error("DEngine - Vulkan: VMA failed to allocate memory for object data.");
	manager.mappedMem = vmaAllocResultInfo.pMappedData;
	if (globUtils.UsingDebugUtils())
	{
		std::string name = std::string("ObjectData - Buffer");
		vk::DebugUtilsObjectNameInfoEXT nameInfo{};
		nameInfo.objectHandle = (u64)(VkBuffer)manager.buffer;
		nameInfo.objectType = vk::ObjectType::eBuffer;
		nameInfo.pObjectName = name.c_str();
		globUtils.debugUtils.setDebugUtilsObjectNameEXT(globUtils.device.handle, nameInfo);
	}


	// Allocate the descriptor-set stuff
	vk::DescriptorPoolSize descrPoolSize{};
	descrPoolSize.descriptorCount = 1;
	descrPoolSize.type = vk::DescriptorType::eUniformBufferDynamic;

	vk::DescriptorPoolCreateInfo descrPoolInfo{};
	descrPoolInfo.maxSets = 1;
	descrPoolInfo.poolSizeCount = 1;
	descrPoolInfo.pPoolSizes = &descrPoolSize;

	manager.descrPool = globUtils.device.createDescriptorPool(descrPoolInfo);
	if (globUtils.UsingDebugUtils())
	{
		std::string name = std::string("ObjectData - DescrPool");
		vk::DebugUtilsObjectNameInfoEXT nameInfo{};
		nameInfo.objectHandle = (u64)(VkDescriptorPool)manager.descrPool;
		nameInfo.objectType = vk::ObjectType::eDescriptorPool;
		nameInfo.pObjectName = name.c_str();
		globUtils.debugUtils.setDebugUtilsObjectNameEXT(globUtils.device.handle, nameInfo);
	}

	vk::DescriptorSetAllocateInfo descrSetAllocInfo{};
	descrSetAllocInfo.descriptorPool = manager.descrPool;
	descrSetAllocInfo.descriptorSetCount = 1;
	descrSetAllocInfo.pSetLayouts = &manager.descrSetLayout;
	vkResult = globUtils.device.allocateDescriptorSets(descrSetAllocInfo, &manager.descrSet);
	if (vkResult != vk::Result::eSuccess)
		throw std::runtime_error("DEngine - Vulkan: Unable to allocate descriptor set for object-data.");
	if (globUtils.UsingDebugUtils())
	{
		std::string name = std::string("ObjectData - DescrSet");
		vk::DebugUtilsObjectNameInfoEXT nameInfo{};
		nameInfo.objectHandle = (u64)(VkDescriptorSet)manager.descrSet;
		nameInfo.objectType = vk::ObjectType::eDescriptorSet;
		nameInfo.pObjectName = name.c_str();
		globUtils.debugUtils.setDebugUtilsObjectNameEXT(globUtils.device.handle, nameInfo);
	}

	// Write to the descriptor set
	vk::DescriptorBufferInfo descrBuffInfo{};
	descrBuffInfo.buffer = manager.buffer;
	descrBuffInfo.offset = 0;
	descrBuffInfo.range = manager.minElementSize;

	vk::WriteDescriptorSet write{};
	write.descriptorCount = 1;
	write.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
	write.dstBinding = 0;
	write.dstSet = manager.descrSet;
	write.pBufferInfo = &descrBuffInfo;
	globUtils.device.updateDescriptorSets(write, nullptr);

}

void DEngine::Gfx::Vk::ObjectDataManager::Update(
	ObjectDataManager& manager,
	Std::Span<Math::Mat4 const> transforms,
	u8 currentInFlightIndex)
{
	DENGINE_DETAIL_GFX_ASSERT(transforms.Size() <= manager.capacity);

	char* dstResourceSet = (char*)manager.mappedMem + manager.capacity * manager.elementSize * currentInFlightIndex;
	for (uSize i = 0; i < transforms.Size(); i += 1)
	{
		char* dst = dstResourceSet + manager.elementSize * i;
		std::memcpy(dst, transforms[i].Data(), 64);
	}
}

bool DEngine::Gfx::Vk::ObjectDataManager::Init(
	ObjectDataManager& manager,
	uSize minUniformBufferOffsetAlignment,
	u8 resourceSetCount,
	VmaAllocator vma,
	DeviceDispatch const& device,
	DebugUtilsDispatch const* debugUtils)
{
	vk::Result vkResult{};

	manager.elementSize = Math::Max(
		ObjectDataManager::minElementSize,
		minUniformBufferOffsetAlignment);
	manager.capacity = ObjectDataManager::minCapacity;

	// Allocate the buffer
	vk::BufferCreateInfo buffInfo{};
	buffInfo.sharingMode = vk::SharingMode::eExclusive;
	buffInfo.size = manager.elementSize * manager.capacity * resourceSetCount;
	buffInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer;
	VmaAllocationCreateInfo vmaAllocInfo{};
	vmaAllocInfo.flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_MAPPED_BIT;
	vmaAllocInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU;
	VmaAllocationInfo vmaAllocResultInfo{};
	vkResult = (vk::Result)vmaCreateBuffer(
		vma,
		(VkBufferCreateInfo const*)&buffInfo,
		&vmaAllocInfo,
		(VkBuffer*)&manager.buffer,
		&manager.vmaAlloc,
		&vmaAllocResultInfo);
	if (vkResult != vk::Result::eSuccess)
		throw std::runtime_error("DEngine, Vulkan: VMA failed to allocate memory for object data.");
	manager.mappedMem = vmaAllocResultInfo.pMappedData;
	if (debugUtils)
	{
		std::string name = std::string("ObjectData - Buffer");
		vk::DebugUtilsObjectNameInfoEXT nameInfo{};
		nameInfo.objectHandle = (u64)(VkBuffer)manager.buffer;
		nameInfo.objectType = vk::ObjectType::eBuffer;
		nameInfo.pObjectName = name.c_str();
		debugUtils->setDebugUtilsObjectNameEXT(device.handle, nameInfo);
	}

	// Create descriptor set layout
	vk::DescriptorSetLayoutBinding objectDataBinding{};
	objectDataBinding.binding = 0;
	objectDataBinding.descriptorCount = 1;
	objectDataBinding.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
	objectDataBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;
	vk::DescriptorSetLayoutCreateInfo descrSetLayoutInfo{};
	descrSetLayoutInfo.bindingCount = 1;
	descrSetLayoutInfo.pBindings = &objectDataBinding;
	manager.descrSetLayout = device.createDescriptorSetLayout(descrSetLayoutInfo);
	if (debugUtils)
	{
		std::string name = std::string("ObjectData - DescrSetLayout");
		vk::DebugUtilsObjectNameInfoEXT nameInfo{};
		nameInfo.objectHandle = (u64)(VkDescriptorSetLayout)manager.descrSetLayout;
		nameInfo.objectType = vk::ObjectType::eDescriptorSetLayout;
		nameInfo.pObjectName = name.c_str();
		debugUtils->setDebugUtilsObjectNameEXT(device.handle, nameInfo);
	}

	// Allocate the descriptor-set stuff
	vk::DescriptorPoolSize descrPoolSize{};
	descrPoolSize.descriptorCount = 1;
	descrPoolSize.type = vk::DescriptorType::eUniformBufferDynamic;

	vk::DescriptorPoolCreateInfo descrPoolInfo{};
	descrPoolInfo.maxSets = 1;
	descrPoolInfo.poolSizeCount = 1;
	descrPoolInfo.pPoolSizes = &descrPoolSize;

	manager.descrPool = device.createDescriptorPool(descrPoolInfo);
	if (debugUtils)
	{
		std::string name = std::string("ObjectData - DescrPool");
		vk::DebugUtilsObjectNameInfoEXT nameInfo{};
		nameInfo.objectHandle = (u64)(VkDescriptorPool)manager.descrPool;
		nameInfo.objectType = vk::ObjectType::eDescriptorPool;
		nameInfo.pObjectName = name.c_str();
		debugUtils->setDebugUtilsObjectNameEXT(device.handle, nameInfo);
	}

	vk::DescriptorSetAllocateInfo descrSetAllocInfo{};
	descrSetAllocInfo.descriptorPool = manager.descrPool;
	descrSetAllocInfo.descriptorSetCount = 1;
	descrSetAllocInfo.pSetLayouts = &manager.descrSetLayout;
	vkResult = device.allocateDescriptorSets(descrSetAllocInfo, &manager.descrSet);
	if (vkResult != vk::Result::eSuccess)
		throw std::runtime_error("DEngine - Vulkan: Unable to allocate descriptor set for object-data.");
	if (debugUtils)
	{
		std::string name = std::string("ObjectData - DescrSet");
		vk::DebugUtilsObjectNameInfoEXT nameInfo{};
		nameInfo.objectHandle = (u64)(VkDescriptorSet)manager.descrSet;
		nameInfo.objectType = vk::ObjectType::eDescriptorSet;
		nameInfo.pObjectName = name.c_str();
		debugUtils->setDebugUtilsObjectNameEXT(device.handle, nameInfo);
	}

	// Write to the descriptor set
	vk::DescriptorBufferInfo descrBuffInfo{};
	descrBuffInfo.buffer = manager.buffer;
	descrBuffInfo.offset = 0;
	descrBuffInfo.range = manager.minElementSize;

	vk::WriteDescriptorSet write{};
	write.descriptorCount = 1;
	write.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
	write.dstBinding = 0;
	write.dstSet = manager.descrSet;
	write.pBufferInfo = &descrBuffInfo;
	device.updateDescriptorSets(write, nullptr);

	return true;
}