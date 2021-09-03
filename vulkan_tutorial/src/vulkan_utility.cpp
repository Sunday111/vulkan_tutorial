#include "vulkan_utility.h"

#include <cassert>

#include "macro.h"
#include "integer.h"
#include "error_handling.h"

template<auto fn, typename Element, typename... FunctionArgs>
void vk_get_array(const std::string_view& name, std::vector<Element>& elements, FunctionArgs&&... function_args)
{
    ui32 num_elements = 0;
    vk_expect_success(fn(function_args..., &num_elements, nullptr), "{}: get count", name);
    elements.resize(num_elements);

    [[likely]]
    if (num_elements > 0)
    {
        vk_expect_success(fn(function_args..., &num_elements, elements.data()), "{}: get data", name);
    }
}

#define VK_GET_ARRAY(fn, arr, ...) vk_get_array<fn>(#fn, arr, __VA_ARGS__);

void VulkanUtility::get_devices(VkInstance instance, std::vector<VkPhysicalDevice>& out_devices) noexcept
{
    VK_GET_ARRAY(vkEnumeratePhysicalDevices, out_devices, instance);
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

bool VulkanUtility::device_supports_present(VkPhysicalDevice device, ui32 queue_family_index, VkSurfaceKHR surface)
{
    VkBool32 present_supported = false;
    vk_expect_success(
        vkGetPhysicalDeviceSurfaceSupportKHR(device, queue_family_index, surface, &present_supported),
        "vkGetPhysicalDeviceSurfaceSupportKHR");
    return present_supported;
}

void VulkanUtility::get_device_extensions(VkPhysicalDevice device, std::vector<VkExtensionProperties>& extensions)
{
    VK_GET_ARRAY(vkEnumerateDeviceExtensionProperties, extensions, device, nullptr);
}

void VulkanUtility::get_device_surface_formats(VkPhysicalDevice device, VkSurfaceKHR surface, std::vector<VkSurfaceFormatKHR>& out_formats)
{
    VK_GET_ARRAY(vkGetPhysicalDeviceSurfaceFormatsKHR, out_formats, device, surface);
}

void VulkanUtility::get_device_surface_present_modes(VkPhysicalDevice device, VkSurfaceKHR surface, std::vector<VkPresentModeKHR>& modes)
{
    VK_GET_ARRAY(vkGetPhysicalDeviceSurfacePresentModesKHR, modes, device, surface);
}

void VulkanUtility::get_swap_chain_images(VkDevice device, VkSwapchainKHR swap_chain, std::vector<VkImage>& images)
{
    VK_GET_ARRAY(vkGetSwapchainImagesKHR, images, device, swap_chain);
}

std::string_view VulkanUtility::serveriity_to_string(VkDebugUtilsMessageSeverityFlagBitsEXT severity)
{
    switch (severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: return "verbose";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: return "info";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: return "warning";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: return "error";
    default:
        assert(false);
        return "unknown severity";
    }
}

std::string_view VulkanUtility::vk_result_to_string(VkResult vk_result) noexcept
{
#define BCASE(x) case x: return TOSTRING(x)
    switch (vk_result)
    {
        BCASE(VK_SUCCESS);
        BCASE(VK_NOT_READY);
        BCASE(VK_TIMEOUT);
        BCASE(VK_EVENT_SET);
        BCASE(VK_EVENT_RESET);
        BCASE(VK_INCOMPLETE);
        BCASE(VK_ERROR_OUT_OF_HOST_MEMORY);
        BCASE(VK_ERROR_OUT_OF_DEVICE_MEMORY);
        BCASE(VK_ERROR_INITIALIZATION_FAILED);
        BCASE(VK_ERROR_DEVICE_LOST);
        BCASE(VK_ERROR_MEMORY_MAP_FAILED);
        BCASE(VK_ERROR_LAYER_NOT_PRESENT);
        BCASE(VK_ERROR_EXTENSION_NOT_PRESENT);
        BCASE(VK_ERROR_FEATURE_NOT_PRESENT);
        BCASE(VK_ERROR_INCOMPATIBLE_DRIVER);
        BCASE(VK_ERROR_TOO_MANY_OBJECTS);
        BCASE(VK_ERROR_FORMAT_NOT_SUPPORTED);
        BCASE(VK_ERROR_FRAGMENTED_POOL);
        BCASE(VK_ERROR_UNKNOWN);
        BCASE(VK_ERROR_OUT_OF_POOL_MEMORY);
        BCASE(VK_ERROR_INVALID_EXTERNAL_HANDLE);
        BCASE(VK_ERROR_SURFACE_LOST_KHR);
        BCASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
        BCASE(VK_SUBOPTIMAL_KHR);
        BCASE(VK_ERROR_OUT_OF_DATE_KHR);
        BCASE(VK_ERROR_VALIDATION_FAILED_EXT);
        
        default:
            assert(false);
            return "UNKNOWN ERROR CODE";
    }

#undef BCASE
}

#undef VK_GET_ARRAY