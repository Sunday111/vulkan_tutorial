#include "device_surface_info.h"

#include "vulkan_utility.h"
#include "error_handling.h"

void DeviceSurfaceInfo::Populate(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    VkWrap(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)(device, surface, &capabilities);
    VulkanUtility::GetDeviceSurfaceFormats(device, surface, formats);
    VulkanUtility::GetDeviceSurfacePresentMode(device, surface, present_modes);
}

VkSurfaceFormatKHR DeviceSurfaceInfo::ChooseSurfaceFormat(const VkSurfaceFormatKHR& preferred) const noexcept
{
    size_t best_index = 0;
    int best_score = -1;

    for (size_t index = 0; index != formats.size(); ++index)
    {
        const auto& surface_format = formats[index];

        int score = 0;
        if (surface_format.format == preferred.format)
        {
            ++score;
        }

        if (surface_format.colorSpace == preferred.colorSpace)
        {
            ++score;
        }

        if (score > best_score)
        {
            best_index = index;
            best_score = score;

            // full match detected
            if (best_score == 2)
            {
                break;
            }
        }
    }

    return formats[best_index];
}
