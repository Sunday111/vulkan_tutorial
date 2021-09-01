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
};