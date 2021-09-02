#pragma once

#include <vector>

#include "integer.h"
#include "vulkan/vulkan.h"

class VulkanUtility
{
public:
    static void get_devices(VkInstance instance, std::vector<VkPhysicalDevice>& out_devices) noexcept;
    static void get_queue_families(VkPhysicalDevice device, std::vector<VkQueueFamilyProperties>& out_queue_families) noexcept;
    [[nodiscard]] static bool device_supports_present(VkPhysicalDevice device, ui32 queue_family_index, VkSurfaceKHR surface);
    static void get_device_extensions(VkPhysicalDevice device, std::vector<VkExtensionProperties>& out_extensions);
    static void get_device_surface_formats(VkPhysicalDevice device, VkSurfaceKHR surface, std::vector<VkSurfaceFormatKHR>& out_formats);
    static void get_device_surface_present_modes(VkPhysicalDevice device, VkSurfaceKHR surface, std::vector<VkPresentModeKHR>& out_modes);
    static void get_swap_chain_images(VkDevice device, VkSwapchainKHR swap_chain, std::vector<VkImage>& out_images);
};