#include "physical_device_info.h"

#include "vulkan_utility.h"

void PhysicalDeviceInfo::set_device(VkPhysicalDevice new_device) noexcept
{
    [[likely]]
    if (device != new_device)
    {
        if (new_device != VK_NULL_HANDLE)
        {
            device = new_device;
            vkGetPhysicalDeviceProperties(device, &properties);
            vkGetPhysicalDeviceFeatures(device, &features);
            VulkanUtility::get_queue_families(device, families_properties);
            populate_index_cache();
        }
        else
        {
            *this = PhysicalDeviceInfo();
        }
    }
}

void PhysicalDeviceInfo::populate_index_cache() noexcept
{
    int i = 0;
    for (const auto& queueFamily : families_properties)
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            queue_family_index_cache.graphics = i;
        }

        if (queue_family_index_cache.is_complete())
        {
            break;
        }

        i++;
    }
}

[[nodiscard]] int PhysicalDeviceInfo::rate_device() const noexcept
{
    if (!queue_family_index_cache.has_all_required())
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
