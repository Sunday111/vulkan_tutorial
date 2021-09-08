#pragma once

#include <string_view>
#include <vector>
#include <span>

#include "integer.h"
#include "vulkan/vulkan.h"

class VulkanUtility
{
public:
    [[nodiscard]] static std::string_view ResultToString(VkResult vk_result) noexcept;
    static void GetDevices(VkInstance instance, std::vector<VkPhysicalDevice>& out_devices) noexcept;
    static void GetQueueFamilies(VkPhysicalDevice device, std::vector<VkQueueFamilyProperties>& out_queue_families) noexcept;
    [[nodiscard]] static bool DeviceSupportsPresentation(VkPhysicalDevice device, ui32 queue_family_index, VkSurfaceKHR surface);
    static void GetDeviceExtensions(VkPhysicalDevice device, std::vector<VkExtensionProperties>& out_extensions);
    static void GetDeviceSurfaceFormats(VkPhysicalDevice device, VkSurfaceKHR surface, std::vector<VkSurfaceFormatKHR>& out_formats);
    static void GetDeviceSurfacePresentMode(VkPhysicalDevice device, VkSurfaceKHR surface, std::vector<VkPresentModeKHR>& out_modes);
    static void GetSwapChainImages(VkDevice device, VkSwapchainKHR swap_chain, std::vector<VkImage>& out_images);
    static void GetInstanceLayerProperties(std::vector<VkLayerProperties>& out_properties);
    [[nodiscard]] static std::string_view SeverityToString(VkDebugUtilsMessageSeverityFlagBitsEXT severity);
    static void FreeMemory(VkDevice device, VkDeviceMemory& memory, const VkAllocationCallbacks* allocation_callbacks = nullptr) noexcept;
    static void FreeMemory(VkDevice device, std::span<VkDeviceMemory> memory, const VkAllocationCallbacks* allocation_callbacks = nullptr) noexcept;
    static void MapCopyUnmap(const void* data, VkDeviceSize size, VkDevice device, VkDeviceMemory device_memory, VkDeviceSize offset = 0, VkMemoryMapFlags mem_map_flags = 0);

    template<auto fn, typename Handle>
    static void Destroy(Handle& handle, const VkAllocationCallbacks* allocation_callbacks = nullptr) noexcept
    {
        if (handle)
        {
            fn(handle, allocation_callbacks);
            handle = nullptr;
        }
    }

    template<auto fn, typename Owner, typename Handle>
    static void Destroy(Owner& owner, Handle& handle, const VkAllocationCallbacks* allocation_callbacks = nullptr) noexcept
    {
        if (handle)
        {
            fn(owner, handle, allocation_callbacks);
            handle = nullptr;
        }
    }

    template<auto fn, typename Owner, typename Handle>
    static void Destroy(Owner& owner, std::vector<Handle>& handles, const VkAllocationCallbacks* allocation_callbacks = nullptr)
    {
        while (!handles.empty())
        {
            Destroy<fn>(owner, handles.back(), allocation_callbacks);
            handles.pop_back();
        }
    }

    template<typename T>
    static void MapCopyUnmap(T& object, VkDevice device, VkDeviceMemory device_memory,
        VkDeviceSize offset = 0, VkMemoryMapFlags mem_map_flags = 0)
    {
        MapCopyUnmap(&object, sizeof(T), device, device_memory, offset, mem_map_flags);
    }

    [[nodiscard]] static constexpr bool FormatHasStencilComponent(VkFormat format) noexcept
    {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }
};