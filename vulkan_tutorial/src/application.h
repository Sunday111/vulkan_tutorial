#pragma once

#include <string>
#include <vector>
#include <memory>

#include "integer.h"
#include "vulkan/vulkan.h"
#include "device/vulkan_device.h"

struct GLFWwindow;

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
    [[nodiscard]] VkPhysicalDevice pick_physical_device() const;
    void create_device();
    void checkValidationLayerSupport();
    void initialize_vulkan();
    void create_instance();
    void initialize_window();
    void main_loop();
    void cleanup();
    std::vector<const char*> get_required_extensions();

private:
    std::vector<std::string> validation_layers_;
    GLFWwindow* window_ = nullptr;
    VkInstance vk_instance_;
    ui32 window_width_ = kDefaultWindowWidth;
    ui32 window_height_ = kDefaultWindowHeight;

    VulkanDevice device_;

    ui8 glfw_initialized : 1;
};
