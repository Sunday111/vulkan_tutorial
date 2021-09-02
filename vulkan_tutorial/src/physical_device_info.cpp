#include "physical_device_info.h"

#include "vulkan_utility.h"
#include "error_handling.h"

void PhysicalDeviceInfo::populate(VkPhysicalDevice new_device, VkSurfaceKHR surface)
{
    device = new_device;
    vkGetPhysicalDeviceProperties(device, &properties);
    vkGetPhysicalDeviceFeatures(device, &features);
    VulkanUtility::get_queue_families(device, families_properties);
    VulkanUtility::get_device_extensions(device, extensions);
    populate_index_cache(surface);
}

bool PhysicalDeviceInfo::has_extension(std::string_view name) const noexcept
{
    for (auto& ext : extensions)
    {
        if (name == ext.extensionName)
        {
            return true;
        }
    }

    return false;
}

void PhysicalDeviceInfo::populate_index_cache(VkSurfaceKHR surface)
{
    int i = 0;
    for (const auto& queueFamily : families_properties)
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            graphics_fi_ = i;
        }

        if (VulkanUtility::device_supports_present(device, i, surface))
        {
            present_fi_ = i;
        }

        if (is_complete())
        {
            break;
        }

        i++;
    }
}

[[nodiscard]] int PhysicalDeviceInfo::rate_device() const noexcept
{
    if (!has_all_required())
    {
        return -1;
    }

    int score = 0;

    // Discrete GPUs have a significant performance advantage
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
        score += 1000;
    }

    return score;
}
