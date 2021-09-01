#pragma once

#include <string>
#include <vector>

#include "integer.h"
#include "vulkan/vulkan.h"

struct GLFWwindow;

struct QueueFamilyIndexCache
{
    [[nodiscard]] bool has_graphics_family() const noexcept { return graphics != -1; }
    [[nodiscard]] bool has_all_required() const noexcept { return has_graphics_family(); }
    [[nodiscard]] bool has_all_optional() const noexcept { return true; }
    [[nodiscard]] bool is_complete() const noexcept { return has_all_required() && has_all_optional(); }

    int graphics = -1;
};

struct DeviceInfo
{
    void set_device(VkPhysicalDevice new_device) noexcept;
    int rate_device() const noexcept;
    void populate_index_cache() noexcept;

    VkPhysicalDevice device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    std::vector<VkQueueFamilyProperties> families_properties;
    QueueFamilyIndexCache queue_family_index_cache;
};

class Application
{
public:
    static constexpr ui32 kDefaultWindowWidth = 800;
    static constexpr ui32 kDefaultWindowHeight = 600;

public:
    Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    ~Application();

    void run();

private:
    void setup_debug_messenger();
    void pick_physical_device();
    void checkValidationLayerSupport();
    void initialize_vulkan();
    void create_instance();
    void initialize_window();
    void main_loop();
    void cleanup();
    std::vector<const char*> get_required_extensions();

private:
    VkDebugUtilsMessengerEXT debug_messenger_;
    std::vector<std::string> validation_layers_;
    GLFWwindow* window_ = nullptr;
    VkInstance vk_instance_;
    VkPhysicalDevice vk_device_ = VK_NULL_HANDLE;
    ui32 window_width_ = kDefaultWindowWidth;
    ui32 window_height_ = kDefaultWindowHeight;

    ui8 glfw_initialized : 1;
};
