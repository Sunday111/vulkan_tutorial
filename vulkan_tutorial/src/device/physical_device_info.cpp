#include "physical_device_info.h"

#include "vulkan_utility.h"
#include "error_handling.h"

void PhysicalDeviceInfo::set_device(VkPhysicalDevice new_device, VkSurfaceKHR surface)
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
            VulkanUtility::get_device_extensions(device, extensions);

            vk_expect_success(
                vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapchain.capabilities),
                "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

            VulkanUtility::get_device_surface_formats(device, surface, swapchain.formats);
            VulkanUtility::get_device_surface_present_modes(device, surface, swapchain.present_modes);

            populate_index_cache(surface);
        }
        else
        {
            *this = PhysicalDeviceInfo();
        }
    }
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

VkSurfaceFormatKHR SwapChainSupportDetails::choose_surface_format(const VkSurfaceFormatKHR& preferred) const noexcept
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
