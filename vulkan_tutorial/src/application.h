#pragma once

#include <array>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <optional>
#include <span>

#include "vulkan/vulkan.h"

#include "integer.h"
#include "physical_device_info.h"
#include "device_surface_info.h"

#include "glm/glm.hpp"
#include "error_handling.h"

template<typename T>
struct StructDescriptor
{
};

struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;
};

template<>
struct StructDescriptor<Vertex>
{
    [[nodiscard]] static constexpr std::array<VkVertexInputBindingDescription, 1> get_binding_description() noexcept
    {
        VkVertexInputBindingDescription binding_description{};

        binding_description.binding = 0;
        binding_description.stride = sizeof(Vertex);
        binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return std::array{ binding_description };
    }

    [[nodiscard]] static constexpr std::array<VkVertexInputAttributeDescription, 2> get_input_attribute_descriptions() noexcept
    {
        VkVertexInputAttributeDescription d0{};
        d0.binding = 0;
        d0.location = 0;
        d0.format = VK_FORMAT_R32G32_SFLOAT;
        d0.offset = offsetof(Vertex, pos);

        VkVertexInputAttributeDescription d1{};
        d1.binding = 0;
        d1.location = 1;
        d1.format = VK_FORMAT_R32G32B32_SFLOAT;
        d1.offset = offsetof(Vertex, color);

        return std::array{d0, d1};
    }
};

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, { 1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f}, { 0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f}, { 0.0f, 0.0f, 1.0f}},
    {{-0.5f,  0.5f}, { 1.0f, 1.0f, 1.0f}}
};

const std::vector<ui16> indices = {
    0, 1, 2, 2, 3, 0
};

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
    [[nodiscard]] VkCommandPool create_command_pool(ui32 queue_family_index, VkCommandPoolCreateFlags flags = 0) const;
    void create_command_pools();
    void create_vertex_buffers();
    void create_index_buffers();
    void create_command_buffers();
    void create_sync_objects();
    VkShaderModule create_shader_module(const std::filesystem::path& file, std::vector<char>& cache);
    void check_required_layers_support();
    void initialize_vulkan();
    void create_instance();
    void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& buffer_memory) const;
    void copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);

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

    void create_gpu_buffer_raw(const void* data, VkDeviceSize buffer_size,
        VkBufferUsageFlags usage_flags, VkBuffer& buffer, VkDeviceMemory& buffer_memory);

    template<typename T>
    void create_gpu_buffer(std::span<const T> view, VkBufferUsageFlags usage_flags,
        VkBuffer& buffer, VkDeviceMemory& buffer_memory)
    {
        const VkDeviceSize buffer_size = sizeof(T) * view.size();
        create_gpu_buffer_raw(view.data(), buffer_size, usage_flags, buffer, buffer_memory);
    }

private:
    std::filesystem::path executable_file_;
    std::vector<VkImage> swap_chain_images_;
    std::vector<VkImageView> swap_chain_image_views_;
    std::vector<const char*> required_layers_;
    std::vector<const char*> device_extensions_;
    std::vector<VkFramebuffer> swap_chain_frame_buffers_;
    std::vector<VkCommandBuffer> command_buffers_;
    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> in_flight_fences_; // indexed by current frame
    std::vector<VkFence> images_in_flight_; // indexed by image index
    std::unique_ptr<DeviceSurfaceInfo> surface_info_;
    std::unique_ptr<PhysicalDeviceInfo> device_info_;
    VkDeviceMemory vertex_buffer_memory_ = nullptr;
    VkBuffer vertex_buffer_ = nullptr;
    VkDeviceMemory index_buffer_memory_ = nullptr;
    VkBuffer index_buffer_ = nullptr;
    VkQueue graphics_queue_ = nullptr;
    VkQueue present_queue_ = nullptr;
    VkCommandPool persistent_command_pool_ = nullptr;
    VkCommandPool transient_command_pool_ = nullptr;
    VkPipeline graphics_pipeline_ = nullptr;
    VkRenderPass render_pass_ = nullptr;
    VkPipelineLayout pipeline_layout_ = nullptr;
    VkSwapchainKHR swap_chain_ = nullptr;
    VkSurfaceKHR surface_ = nullptr;
    VkDevice device_ = nullptr;
    GLFWwindow* window_ = nullptr;
    VkInstance instance_ = nullptr;
    size_t current_frame_ = 0;
    VkExtent2D swap_chain_extent_ = {};
    ui32 window_width_ = kDefaultWindowWidth;
    ui32 window_height_ = kDefaultWindowHeight;
    VkFormat swap_chain_image_format_ = {};
    ui8 glfw_initialized_ : 1;
    ui8 frame_buffer_resized_ : 1;
};
