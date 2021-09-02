#pragma once

#include <string>
#include <vector>
#include <memory>

#include "integer.h"
#include "vulkan/vulkan.h"
#include "device/vulkan_device.h"
#include "device/physical_device_info.h"

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
    void pick_physical_device();
    void create_surface();
    void create_device();
    void create_swap_chain();
    void checkValidationLayerSupport();
    void initialize_vulkan();
    void create_instance();
    void initialize_window();
    void main_loop();
    void cleanup();
    VkSurfaceFormatKHR choose_surface_format() const;
    VkPresentModeKHR choose_present_mode() const;
    VkExtent2D choose_swap_extent() const;
    std::vector<const char*> get_required_extensions();

private:
    std::vector<VkImage> swap_chain_images_;
    std::vector<const char*> validation_layers_;
    std::vector<const char*> device_extensions_;
    VkSwapchainKHR swap_chain_ = nullptr;
    VkSurfaceKHR surface_ = nullptr;
    PhysicalDeviceInfo device_info_;
    VulkanDevice device_;
    GLFWwindow* window_ = nullptr;
    VkInstance vk_instance_ = nullptr;
    ui32 window_width_ = kDefaultWindowWidth;
    ui32 window_height_ = kDefaultWindowHeight;
    VkFormat swap_chain_image_format_;
    VkExtent2D swap_chain_extent_;
    ui8 glfw_initialized : 1;
};
