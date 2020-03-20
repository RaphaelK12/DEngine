#pragma once

#include "DEngine/FixedWidthTypes.hpp"
#include "DEngine/Containers/Span.hpp"
#include "DEngine/Containers/Optional.hpp"
#include "DEngine/Containers/FixedVector.hpp"

#include "DEngine/Math/Matrix/Matrix.hpp"
#include "DEngine/Math/Common.hpp"

namespace DEngine::Gfx
{
	namespace Constants
	{
		constexpr u8 maxViewportCount = 8;
	}

	class IWsi;
	class ILog;
	struct InitInfo;
	class ViewportRef;
	struct Draw_Params;

	struct ViewportUpdateData
	{
		uSize id = static_cast<uSize>(-1);
		u32 width = 0;
		u32 height = 0;
		Math::Mat4 transform{};
	};

	struct Draw_Params
	{
		bool presentMainWindow = false;
		bool resizeEvent = false;
		
		Cont::FixedVector<ViewportUpdateData, 10> viewportUpdates = {};
	};

	class Data
	{
	public:
		Data(Data&&) noexcept = default;

		ViewportRef NewViewport();
		void DeleteViewport(uSize viewportID);
		uSize GetViewportCount();

		void Draw(Draw_Params const& params);

	private:
		Data() = default;
		Data(Data const&) = delete;

		ILog* iLog = nullptr;
		IWsi* iWsi = nullptr;

		void* apiDataBuffer{};

		friend Cont::Opt<Data> Initialize(const InitInfo& initInfo);
	};

	struct InitInfo
	{
		u32 maxWidth = 0;
		u32 maxHeight = 0;

		ILog* optional_iLog = nullptr;
		IWsi* iWsi = nullptr;
		Cont::Span<char const*> requiredVkInstanceExtensions{};
	};

	class ILog
	{
	public:
		virtual ~ILog() {};

		virtual void log(char const* msg) = 0;
	};

	class IWsi
	{
	public:
		virtual ~IWsi() {};

		// Return type is VkResult
		//
		// Argument #1: VkInstance - The Vulkan instance handle
		// Argument #2: VkAllocationCallbacks const* - Allocation callbacks for surface creation.
		// Argument #3: VkSurfaceKHR* - The output surface handle
		virtual i32 createVkSurface(u64 vkInstance, void const* allocCallbacks, u64* outSurface) = 0;
	};

	class ViewportRef
	{
	public:
		ViewportRef() = default;

		[[nodiscard]] bool IsValid() const { return viewportID != 255; }
		[[nodiscard]] uSize ViewportID() const { return viewportID; }
		[[nodiscard]] void* ImGuiTexID() const { return imguiTexID; }

	private:
		uSize viewportID = static_cast<uSize>(-1);
		void* imguiTexID = nullptr;

		friend class Data;
	};

	
	Cont::Opt<Data> Initialize(const InitInfo& initInfo);
}