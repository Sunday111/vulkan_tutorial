#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <optional>

#include "vulkan/vulkan.h"

#include "integer.h"
#include "physical_device_info.h"
#include "device_surface_info.h"

struct GLFWwindow;

class Application
{
public:
    static constexpr size_t kMaxFramesInFlight = 2;
    static constexpr ui32 kDefaultWindowWidth = 800;
    static constexpr ui32 kDefaultWindowHeight = 600;

public:
    Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    ~Application();

    void set_executable_file(std::filesystem::path path);
    void run();

private:
    void recreate_swap_chain();
    void pick_physical_device();
    void create_surface();
    void create_device();
    void create_swap_chain();
    void create_swap_chain_image_views();
    void create_render_pass();
    void create_graphics_pipeline();
    void create_frame_buffers();
    void create_command_pool();
    void create_command_buffers();
    void create_sync_objects();
    VkShaderModule create_shader_module(const std::filesystem::path& file, std::vector<char>& cache);
    void checkValidationLayerSupport();
    void initialize_vulkan();
    void create_instance();

    void initialize_window();
    static void frame_buffer_resize_callback(GLFWwindow* window, int width, int height);

    void main_loop();
    std::optional<ui32> acquire_next_swap_chain_image() const;
    void draw_frame();

    void cleanup();
    void cleanup_swap_chain();
    VkSurfaceFormatKHR choose_surface_format() const;
    VkPresentModeKHR choose_present_mode() const;
    VkExtent2D choose_swap_extent() const;
    std::vector<const char*> get_required_extensions();
    [[nodiscard]] std::filesystem::path get_shaders_dir() const noexcept;

private:
    std::vector<VkImage> swap_chain_images_;
    std::vector<VkImageView> swap_chain_image_views_;
    std::vector<const char*> validation_layers_;
    std::vector<const char*> device_extensions_;
    std::vector<VkFramebuffer> swap_chain_frame_buffers_;
    std::vector<VkCommandBuffer> command_buffers_;
    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> in_flight_fences_; // indexed by current frame
    std::vector<VkFence> images_in_flight_; // indexed by image index
    std::filesystem::path executable_file_;
    VkQueue graphics_queue_ = nullptr;
    VkQueue present_queue_ = nullptr;
    VkCommandPool command_pool_ = nullptr;
    VkPipeline graphics_pipeline_ = nullptr;
    VkRenderPass render_pass_ = nullptr;
    VkPipelineLayout pipeline_layout_ = nullptr;
    VkSwapchainKHR swap_chain_ = nullptr;
    VkSurfaceKHR surface_ = nullptr;
    PhysicalDeviceInfo device_info_;
    DeviceSurfaceInfo surface_info_;
    VkDevice device_ = nullptr;
    GLFWwindow* window_ = nullptr;
    VkInstance instance_ = nullptr;
    size_t current_frame_ = 0;
    ui32 window_width_ = kDefaultWindowWidth;
    ui32 window_height_ = kDefaultWindowHeight;
    VkFormat swap_chain_image_format_ = {};
    VkExtent2D swap_chain_extent_ = {};
    ui8 glfw_initialized_ : 1;
    ui8 frame_buffer_resized_ : 1;
};
