#pragma once

#include <optional>
#include <vector>
#include <span>
#include <string_view>

#include "vulkan/vulkan.h"
#include "integer.h"

class PhysicalDeviceInfo
{
public:
    void Populate(VkPhysicalDevice new_device, VkSurfaceKHR present_surface);
    [[nodiscard]] bool HasExtension(std::string_view name) const noexcept;
    [[nodiscard]] int RateDevice() const noexcept;
    void PopulateIndexCache(VkSurfaceKHR surface);

    [[nodiscard]] bool HasGraphicsFamily() const noexcept { return graphics_fi_ != -1; }
    [[nodiscard]] bool HasPresentFamily() const noexcept { return present_fi_ != -1; }
    [[nodiscard]] bool HasAllRequired() const noexcept { return HasGraphicsFamily() && HasPresentFamily(); }
    [[nodiscard]] bool HasAllOptional() const noexcept { return true; }
    [[nodiscard]] bool IsComplete() const noexcept { return HasAllRequired() && HasAllOptional(); }
    [[nodiscard]] ui32 GetGraphicsQueueFamilyIndex() const noexcept { return static_cast<ui32>(graphics_fi_); }
    [[nodiscard]] ui32 GetPresentQueueFamilyIndex() const noexcept { return static_cast<ui32>(present_fi_); }
    [[nodiscard]] std::optional<ui32> FindMemoryTypeIndex(ui32 filter, VkMemoryPropertyFlags properties) const noexcept;
    [[nodiscard]] ui32 GetMemoryTypeIndex(ui32 filter, VkMemoryPropertyFlags properties) const;


    [[nodiscard]] std::optional<VkFormat> FindSupportedFormat(std::span<const VkFormat> candidates, VkImageTiling tiling, VkFormatFeatureFlags features) noexcept;
    [[nodiscard]] VkFormat GetSupportedFormat(std::span<const VkFormat> candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

public:
    std::vector<VkQueueFamilyProperties> families_properties;
    std::vector<VkExtensionProperties> extensions;
    VkPhysicalDevice device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory_properties;
    int graphics_fi_ = -1;
    int present_fi_ = -1;
};
