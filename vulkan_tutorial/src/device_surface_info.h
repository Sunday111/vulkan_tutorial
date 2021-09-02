#pragma once

#include <vector>

#include "vulkan/vulkan.h"

struct DeviceSurfaceInfo
{
    void populate(VkPhysicalDevice device, VkSurfaceKHR surface);
    // chooses surface format which is closest to preferred
    // must be called only if 'formats' is not empty, otherwise - UB
    [[nodiscard]] VkSurfaceFormatKHR choose_surface_format(const VkSurfaceFormatKHR& preferred) const noexcept;

    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};
