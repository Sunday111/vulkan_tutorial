#include "device_surface_info.h"

#include "vulkan_utility.h"
#include "error_handling.h"

void DeviceSurfaceInfo::populate(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    using Vk = VulkanUtility;

    vk_expect_success(
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities),
        "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    Vk::get_device_surface_formats(device, surface, formats);
    Vk::get_device_surface_present_modes(device, surface, present_modes);
}

VkSurfaceFormatKHR DeviceSurfaceInfo::choose_surface_format(const VkSurfaceFormatKHR& preferred) const noexcept
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
