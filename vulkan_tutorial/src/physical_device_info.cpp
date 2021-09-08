#include "physical_device_info.h"

#include <stdexcept>

#include "error_handling.h"
#include "vulkan_utility.h"

void PhysicalDeviceInfo::Populate(VkPhysicalDevice new_device,
                                  VkSurfaceKHR surface) {
  device = new_device;
  vkGetPhysicalDeviceProperties(device, &properties);
  vkGetPhysicalDeviceMemoryProperties(device, &memory_properties);
  vkGetPhysicalDeviceFeatures(device, &features);
  VulkanUtility::GetQueueFamilies(device, families_properties);
  VulkanUtility::GetDeviceExtensions(device, extensions);

  PopulateIndexCache(surface);
}

bool PhysicalDeviceInfo::HasExtension(std::string_view name) const noexcept {
  for (auto& ext : extensions) {
    if (name == ext.extensionName) {
      return true;
    }
  }

  return false;
}

std::optional<ui32> PhysicalDeviceInfo::FindMemoryTypeIndex(
    ui32 filter, VkMemoryPropertyFlags properties) const noexcept {
  for (ui32 i = 0; i < memory_properties.memoryTypeCount; ++i) {
    if ((filter & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags &
                                properties) == properties) {
      return i;
    }
  }

  return {};
}

ui32 PhysicalDeviceInfo::GetMemoryTypeIndex(
    ui32 filter, VkMemoryPropertyFlags properties) const {
  [[likely]] if (const std::optional<ui32> index =
                     FindMemoryTypeIndex(filter, properties);
                 index.has_value()) {
    return *index;
  }

  throw std::runtime_error("failed to find memory type index");
}

std::optional<VkFormat> PhysicalDeviceInfo::FindSupportedFormat(
    std::span<const VkFormat> candidates, VkImageTiling tiling,
    VkFormatFeatureFlags features) noexcept {
  for (VkFormat format : candidates) {
    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties(device, format, &format_properties);

    if (tiling == VK_IMAGE_TILING_LINEAR &&
        (format_properties.linearTilingFeatures & features) == features) {
      return format;
    }

    if (tiling == VK_IMAGE_TILING_OPTIMAL &&
        (format_properties.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  return {};
}

VkFormat PhysicalDeviceInfo::GetSupportedFormat(
    std::span<const VkFormat> candidates, VkImageTiling tiling,
    VkFormatFeatureFlags features) {
  [[likely]] if (auto f = FindSupportedFormat(candidates, tiling, features);
                 f.has_value()) {
    return *f;
  }

  throw std::runtime_error("Failed to find supported format");
}

void PhysicalDeviceInfo::PopulateIndexCache(VkSurfaceKHR surface) {
  int i = 0;
  for (const auto& queueFamily : families_properties) {
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      graphics_fi_ = i;
    }

    if (VulkanUtility::DeviceSupportsPresentation(device, i, surface)) {
      present_fi_ = i;
    }

    if (IsComplete()) {
      break;
    }

    i++;
  }
}

[[nodiscard]] int PhysicalDeviceInfo::RateDevice() const noexcept {
  if (!HasAllRequired()) {
    return -1;
  }

  int score = 0;

  // Discrete GPUs have a significant performance advantage
  if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
    score += 1000;
  }

  if (features.samplerAnisotropy) {
    score += 100;
  }

  return score;
}
