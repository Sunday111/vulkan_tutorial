#include "physical_device_info.hpp"

#include <stdexcept>

#include "error_handling.hpp"
#include "vulkan_utility.hpp"

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
    ui32 filter, VkMemoryPropertyFlags required_properties) const noexcept {
  for (ui32 i = 0; i < memory_properties.memoryTypeCount; ++i) {
    if ((filter & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags &
                                required_properties) == required_properties) {
      return i;
    }
  }

  return {};
}

ui32 PhysicalDeviceInfo::GetMemoryTypeIndex(
    ui32 filter, VkMemoryPropertyFlags required_properties) const {
  [[likely]] if (const std::optional<ui32> index =
                     FindMemoryTypeIndex(filter, required_properties);
                 index.has_value()) {
    return *index;
  }

  throw std::runtime_error("failed to find memory type index");
}

const VkFormatProperties& PhysicalDeviceInfo::GetFormatProperties(
    VkFormat format) noexcept {
  auto it = formats_properties.find(format);
  if (it != formats_properties.end()) {
    return it->second;
  }

  VkFormatProperties& format_properties = formats_properties[format];
  vkGetPhysicalDeviceFormatProperties(device, format, &format_properties);
  return format_properties;
}

std::optional<VkFormat> PhysicalDeviceInfo::FindSupportedFormat(
    std::span<const VkFormat> candidates, VkImageTiling tiling,
    VkFormatFeatureFlags required_features) noexcept {
  for (VkFormat format : candidates) {
    const VkFormatProperties& format_properties = GetFormatProperties(format);

    if (tiling == VK_IMAGE_TILING_LINEAR &&
        (format_properties.linearTilingFeatures & required_features) ==
            required_features) {
      return format;
    }

    if (tiling == VK_IMAGE_TILING_OPTIMAL &&
        (format_properties.optimalTilingFeatures & required_features) ==
            required_features) {
      return format;
    }
  }

  return {};
}

VkFormat PhysicalDeviceInfo::GetSupportedFormat(
    std::span<const VkFormat> candidates, VkImageTiling tiling,
    VkFormatFeatureFlags required_features) {
  [[likely]] if (auto f =
                     FindSupportedFormat(candidates, tiling, required_features);
                 f.has_value()) {
    return *f;
  }

  throw std::runtime_error("Failed to find supported format");
}

VkSampleCountFlagBits PhysicalDeviceInfo::GetMaxUsableSampleCount()
    const noexcept {
  const VkSampleCountFlags counts =
      properties.limits.framebufferColorSampleCounts &
      properties.limits.framebufferDepthSampleCounts;

  if (counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
  if (counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
  if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
  if (counts & VK_SAMPLE_COUNT_8_BIT) return VK_SAMPLE_COUNT_8_BIT;
  if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
  if (counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;
  return VK_SAMPLE_COUNT_1_BIT;
}

void PhysicalDeviceInfo::PopulateIndexCache(VkSurfaceKHR surface) {
  int i = 0;
  for (const auto& queueFamily : families_properties) {
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      graphics_fi_ = i;
    }

    if (VulkanUtility::DeviceSupportsPresentation(device, static_cast<ui32>(i),
                                                  surface)) {
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
