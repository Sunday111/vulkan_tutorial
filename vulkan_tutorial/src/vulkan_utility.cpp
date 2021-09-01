#include "vulkan_utility.h"

#include "integer.h"
#include "error_handling.h"

void VulkanUtility::get_devices(VkInstance instance, std::vector<VkPhysicalDevice>& out_devices) noexcept
{
    ui32 num_devices = 0;
    vk_expect_success(
        vkEnumeratePhysicalDevices(instance, &num_devices, nullptr),
        "vkEnumeratePhysicalDevices for devices count");

    out_devices.resize(num_devices);
    if (num_devices > 0)
    {
        vk_expect_success(
            vkEnumeratePhysicalDevices(instance, &num_devices, out_devices.data()),
            "vkEnumeratePhysicalDevices for devices list");
    }
}

void VulkanUtility::get_queue_families(VkPhysicalDevice device, std::vector<VkQueueFamilyProperties>& out_queue_families) noexcept
{
    ui32 num_queue_families = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families, nullptr);

    out_queue_families.resize(num_queue_families);

    [[likely]]
    if (num_queue_families > 0)
    {
        vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families, out_queue_families.data());
    }
}
