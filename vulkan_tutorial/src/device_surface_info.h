#pragma once

#include <vector>

#include "vulkan/vulkan.h"

struct DeviceSurfaceInfo
{
    void Populate(VkPhysicalDevice device, VkSurfaceKHR surface);
    // chooses surface format which is closest to preferred
    // must be called only if 'formats' is not empty, otherwise - UB
    [[nodiscard]] VkSurfaceFormatKHR ChooseSurfaceFormat(const VkSurfaceFormatKHR& preferred) const noexcept;

    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};
