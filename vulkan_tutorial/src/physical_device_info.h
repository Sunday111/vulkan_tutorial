#pragma once

#include <vector>

#include "vulkan/vulkan.h"

struct QueueFamilyIndexCache
{
    [[nodiscard]] bool has_graphics_family() const noexcept { return graphics != -1; }
    [[nodiscard]] bool has_all_required() const noexcept { return has_graphics_family(); }
    [[nodiscard]] bool has_all_optional() const noexcept { return true; }
    [[nodiscard]] bool is_complete() const noexcept { return has_all_required() && has_all_optional(); }

    int graphics = -1;
};

class PhysicalDeviceInfo
{
public:
    void set_device(VkPhysicalDevice new_device) noexcept;
    [[nodiscard]] int rate_device() const noexcept;
    void populate_index_cache() noexcept;

    VkPhysicalDevice device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    std::vector<VkQueueFamilyProperties> families_properties;
    QueueFamilyIndexCache queue_family_index_cache;
};
