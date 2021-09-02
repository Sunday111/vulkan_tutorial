#pragma once

#include <vector>
#include <string_view>

#include "vulkan/vulkan.h"
#include "integer.h"

struct SwapChainSupportDetails
{
    // chooses surface format which is closest to preferred
    // must be called only if 'formats' is not empty, otherwise - UB
    [[nodiscard]] VkSurfaceFormatKHR choose_surface_format(const VkSurfaceFormatKHR& preferred) const noexcept;

    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

class PhysicalDeviceInfo
{
public:
    void set_device(VkPhysicalDevice new_device, VkSurfaceKHR present_surface);
    [[nodiscard]] bool has_extension(std::string_view name) const noexcept;
    [[nodiscard]] int rate_device() const noexcept;
    void populate_index_cache(VkSurfaceKHR surface);

    [[nodiscard]] bool has_graphics_family() const noexcept { return graphics_fi_ != -1; }
    [[nodiscard]] bool has_present_family() const noexcept { return present_fi_ != -1; }
    [[nodiscard]] bool has_all_required() const noexcept { return has_graphics_family() && has_present_family(); }
    [[nodiscard]] bool has_all_optional() const noexcept { return true; }
    [[nodiscard]] bool is_complete() const noexcept { return has_all_required() && has_all_optional(); }
    [[nodiscard]] ui32 get_graphics_queue_family_index() const noexcept { return static_cast<ui32>(graphics_fi_); }
    [[nodiscard]] ui32 get_present_queue_family_index() const noexcept { return static_cast<ui32>(present_fi_); }

public:
    SwapChainSupportDetails swapchain;
    VkPhysicalDevice device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    std::vector<VkQueueFamilyProperties> families_properties;
    std::vector<VkExtensionProperties> extensions;
    int graphics_fi_ = -1;
    int present_fi_ = -1;
};
