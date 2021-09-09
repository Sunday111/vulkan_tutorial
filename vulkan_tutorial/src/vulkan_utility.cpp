#include "vulkan_utility.hpp"

#include <cassert>
#include <cstring>

#include "error_handling.hpp"
#include "integer.hpp"
#include "macro.hpp"

template <auto fn, typename Element, typename... FunctionArgs>
void vk_get_array(const std::string_view& name, std::vector<Element>& elements,
                  const std::string_view& file, int line,
                  FunctionArgs&&... function_args) {
  ui32 num_elements = 0;
  VkExpect(fn(function_args..., &num_elements, nullptr), VK_SUCCESS, name, file,
           line);
  elements.resize(num_elements);

  [[likely]] if (num_elements > 0) {
    VkExpect(fn(function_args..., &num_elements, elements.data()), VK_SUCCESS,
             name, file, line);
  }
}

#define VK_GET_ARRAY(fn, arr, ...) \
  vk_get_array<fn>(#fn, arr, __FILE__, __LINE__, __VA_ARGS__);
#define VK_GET_ARRAY_NO_ARG(fn, arr) \
  vk_get_array<fn>(#fn, arr, __FILE__, __LINE__);

void VulkanUtility::GetDevices(
    VkInstance instance, std::vector<VkPhysicalDevice>& out_devices) noexcept {
  VK_GET_ARRAY(vkEnumeratePhysicalDevices, out_devices, instance);
}

void VulkanUtility::GetQueueFamilies(
    VkPhysicalDevice device,
    std::vector<VkQueueFamilyProperties>& out_queue_families) noexcept {
  ui32 num_queue_families = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families,
                                           nullptr);
  out_queue_families.resize(num_queue_families);

  [[likely]] if (num_queue_families > 0) {
    vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families,
                                             out_queue_families.data());
  }
}

bool VulkanUtility::DeviceSupportsPresentation(VkPhysicalDevice device,
                                               ui32 queue_family_index,
                                               VkSurfaceKHR surface) {
  VkBool32 present_supported = false;
  VkWrap(vkGetPhysicalDeviceSurfaceSupportKHR)(device, queue_family_index,
                                               surface, &present_supported);
  return present_supported;
}

void VulkanUtility::GetDeviceExtensions(
    VkPhysicalDevice device, std::vector<VkExtensionProperties>& extensions) {
  VK_GET_ARRAY(vkEnumerateDeviceExtensionProperties, extensions, device,
               nullptr);
}

void VulkanUtility::GetDeviceSurfaceFormats(
    VkPhysicalDevice device, VkSurfaceKHR surface,
    std::vector<VkSurfaceFormatKHR>& out_formats) {
  VK_GET_ARRAY(vkGetPhysicalDeviceSurfaceFormatsKHR, out_formats, device,
               surface);
}

void VulkanUtility::GetDeviceSurfacePresentMode(
    VkPhysicalDevice device, VkSurfaceKHR surface,
    std::vector<VkPresentModeKHR>& modes) {
  VK_GET_ARRAY(vkGetPhysicalDeviceSurfacePresentModesKHR, modes, device,
               surface);
}

void VulkanUtility::GetSwapChainImages(VkDevice device,
                                       VkSwapchainKHR swap_chain,
                                       std::vector<VkImage>& images) {
  VK_GET_ARRAY(vkGetSwapchainImagesKHR, images, device, swap_chain);
}

void VulkanUtility::GetInstanceLayerProperties(
    std::vector<VkLayerProperties>& properties) {
  VK_GET_ARRAY_NO_ARG(vkEnumerateInstanceLayerProperties, properties);
}

std::string_view VulkanUtility::SeverityToString(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity) {
  switch (severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
      return "verbose";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
      return "info";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
      return "warning";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
      return "error";
    default:
      assert(false);
      return "unknown severity";
  }
}

// TODO: iterate over VkFlags in some generic way
std::string VulkanUtility::SampleCountFlagsToString(
    VkSampleCountFlags flags) noexcept {
  std::string result;
  auto append = [&result](const std::string_view& view) {
    if (!result.empty()) {
      result += " | ";
    }
    result += view;
  };
  if (flags & VK_SAMPLE_COUNT_64_BIT) append("VK_SAMPLE_COUNT_64_BIT");
  if (flags & VK_SAMPLE_COUNT_32_BIT) append("VK_SAMPLE_COUNT_32_BIT");
  if (flags & VK_SAMPLE_COUNT_16_BIT) append("VK_SAMPLE_COUNT_16_BIT");
  if (flags & VK_SAMPLE_COUNT_8_BIT) append("VK_SAMPLE_COUNT_8_BIT");
  if (flags & VK_SAMPLE_COUNT_4_BIT) append("VK_SAMPLE_COUNT_4_BIT");
  if (flags & VK_SAMPLE_COUNT_2_BIT) append("VK_SAMPLE_COUNT_2_BIT");
  if (flags & VK_SAMPLE_COUNT_1_BIT) append("VK_SAMPLE_COUNT_1_BIT");
  return result;
}

std::string_view VulkanUtility::ResultToString(VkResult vk_result) noexcept {
#define BCASE(x) \
  case x:        \
    return TOSTRING(x)
  switch (vk_result) {
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

void VulkanUtility::FreeMemory(
    VkDevice device, VkDeviceMemory& memory,
    const VkAllocationCallbacks* allocation_callbacks) noexcept {
  if (memory) {
    vkFreeMemory(device, memory, allocation_callbacks);
    memory = nullptr;
  }
}

void VulkanUtility::FreeMemory(
    VkDevice device, std::span<VkDeviceMemory> memory,
    const VkAllocationCallbacks* allocation_callbacks) noexcept {
  for (VkDeviceMemory& val : memory) {
    FreeMemory(device, val, allocation_callbacks);
  }
}

void VulkanUtility::MapCopyUnmap(const void* data, VkDeviceSize size,
                                 VkDevice device, VkDeviceMemory device_memory,
                                 VkDeviceSize offset,
                                 VkMemoryMapFlags mem_map_flags) {
  void* mapped = nullptr;
  VkWrap(vkMapMemory)(device, device_memory, offset, size, mem_map_flags,
                      &mapped);
  std::memcpy(mapped, data, size);
  vkUnmapMemory(device, device_memory);
}
