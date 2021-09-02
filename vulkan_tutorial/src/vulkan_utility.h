#pragma once

#include <string_view>
#include <vector>

#include "integer.h"
#include "vulkan/vulkan.h"

class VulkanUtility
{
public:
    [[nodiscard]] static std::string_view vk_result_to_string(VkResult vk_result) noexcept;
    static void get_devices(VkInstance instance, std::vector<VkPhysicalDevice>& out_devices) noexcept;
    static void get_queue_families(VkPhysicalDevice device, std::vector<VkQueueFamilyProperties>& out_queue_families) noexcept;
    [[nodiscard]] static bool device_supports_present(VkPhysicalDevice device, ui32 queue_family_index, VkSurfaceKHR surface);
    static void get_device_extensions(VkPhysicalDevice device, std::vector<VkExtensionProperties>& out_extensions);
    static void get_device_surface_formats(VkPhysicalDevice device, VkSurfaceKHR surface, std::vector<VkSurfaceFormatKHR>& out_formats);
    static void get_device_surface_present_modes(VkPhysicalDevice device, VkSurfaceKHR surface, std::vector<VkPresentModeKHR>& out_modes);
    static void get_swap_chain_images(VkDevice device, VkSwapchainKHR swap_chain, std::vector<VkImage>& out_images);
    [[nodiscard]] static std::string_view serveriity_to_string(VkDebugUtilsMessageSeverityFlagBitsEXT severity);

    template<auto fn, typename Handle>
    static void destroy(Handle& handle, const VkAllocationCallbacks* allocation_callbacks = nullptr) noexcept
    {
        if (handle)
        {
            fn(handle, allocation_callbacks);
            handle = nullptr;
        }
    }

    template<auto fn, typename Owner, typename Handle>
    static void destroy(Owner& owner, Handle& handle, const VkAllocationCallbacks* allocation_callbacks = nullptr) noexcept
    {
        if (handle)
        {
            fn(owner, handle, allocation_callbacks);
            handle = nullptr;
        }
    }

    template<auto fn, typename Owner, typename Handle>
    static void destroy(Owner& owner, std::vector<Handle>& handles, const VkAllocationCallbacks* allocation_callbacks = nullptr)
    {
        while (!handles.empty())
        {
            destroy<fn>(owner, handles.back(), allocation_callbacks);
            handles.pop_back();
        }
    }
};