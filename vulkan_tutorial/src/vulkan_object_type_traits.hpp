#pragma once

#include "vulkan/vulkan.h"

template <typename T>
struct VulkanObjectTypeTraits {};

template <typename T, VkObjectType V>
struct VulkanObjectTypeImpl {
  using Type = T;
  static constexpr VkObjectType Value = V;
};

template <>
struct VulkanObjectTypeTraits<VkDevice>
    : public VulkanObjectTypeImpl<VkDevice, VK_OBJECT_TYPE_DEVICE> {};

template <>
struct VulkanObjectTypeTraits<VkImage>
    : public VulkanObjectTypeImpl<VkImage, VK_OBJECT_TYPE_IMAGE> {};

template <>
struct VulkanObjectTypeTraits<VkImageView>
    : public VulkanObjectTypeImpl<VkImageView, VK_OBJECT_TYPE_IMAGE_VIEW> {};

template <>
struct VulkanObjectTypeTraits<VkBuffer>
    : public VulkanObjectTypeImpl<VkBuffer, VK_OBJECT_TYPE_BUFFER> {};

template <>
struct VulkanObjectTypeTraits<VkBufferView>
    : public VulkanObjectTypeImpl<VkBufferView, VK_OBJECT_TYPE_BUFFER_VIEW> {};

template <>
struct VulkanObjectTypeTraits<VkDeviceMemory>
    : public VulkanObjectTypeImpl<VkDeviceMemory,
                                  VK_OBJECT_TYPE_DEVICE_MEMORY> {};

template <>
struct VulkanObjectTypeTraits<VkSampler>
    : public VulkanObjectTypeImpl<VkSampler, VK_OBJECT_TYPE_SAMPLER> {};