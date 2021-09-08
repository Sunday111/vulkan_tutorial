#pragma once

#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "debug/annotate/vk_annotate.h"
#include "device_surface_info.h"
#include "error_handling.h"
#include "integer.h"
#include "physical_device_info.h"
#include "pipeline/vertex.h"
#include "vulkan/vulkan.h"

struct GLFWwindow;

class Application {
 public:
  using TimePoint = decltype(std::chrono::high_resolution_clock::now());

  static constexpr size_t kMaxFramesInFlight = 2;
  static constexpr ui32 kDefaultWindowWidth = 800;
  static constexpr ui32 kDefaultWindowHeight = 600;

 public:
  Application();
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  ~Application();

  void SetExecutableFile(std::filesystem::path path);
  void Run();

 private:
  void RecreateSwapChain();
  void PickPhysicalDevice();
  void CreateSurface();
  void CreateDevice();
  void CreateSwapChain();
  void CreateSwapChainImageViews();
  void CreateRenderPass();
  void CreateDescriptorSetLayout();
  void CreateGraphicsPipeline();
  void CreateFrameBuffers();
  [[nodiscard]] VkCommandPool CreateCommandPool(
      ui32 queue_family_index, VkCommandPoolCreateFlags flags = 0) const;
  void CreateCommandPools();
  void CreateDepthResources();
  void CreateTextureImages();
  void LoadModel();
  void CreateVertexBuffers();
  void CreateIndexBuffers();
  void CreateUniformBuffers();
  void CreateDescriptorPool();
  void CreateDescriptorSets();
  void CreateCommandBuffers();
  void CreateSyncObjects();
  VkShaderModule CreateShaderModule(const std::filesystem::path& file,
                                    std::vector<char>& cache);
  void CheckRequiredLayersSupport();
  void InitializeVulkan();
  void CreateInstance();
  void SetupDebugMessenger();
  void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags properties, VkBuffer& buffer,
                    VkDeviceMemory& buffer_memory) const;

  void CopyBuffer(VkCommandBuffer command_buffer, VkBuffer src, VkBuffer dst,
                  VkDeviceSize size);
  void CopyBufferToImage(VkCommandBuffer command_buffer, VkBuffer src,
                         VkImage image, ui32 width, ui32 height);
  void TransitionImageLayout(VkCommandBuffer command_buffer, VkImage image,
                             VkFormat format, VkImageLayout old_layout,
                             VkImageLayout new_layout, ui32 mip_levels);
  void GenerateMipMaps_Blit(VkCommandBuffer command_buffer, VkImage image,
                            ui32 width, ui32 height, ui32 mip_levels);

  VkCommandBuffer BeginSingleTimeCommands();
  void EndSingleTimeCommands(VkCommandBuffer command_buffer);

  template <typename T>
  void ExecuteSingleTimeCommands(T&& visitor) {
    const VkCommandBuffer command_buffer = BeginSingleTimeCommands();
    visitor(command_buffer);
    EndSingleTimeCommands(command_buffer);
  }

  void InitializeWindow();
  static void FrameBufferResizeCallback(GLFWwindow* window, int width,
                                        int height);

  void MainLoop();
  std::optional<ui32> AcquireNextSwapChainImage() const;
  void DrawFrame();
  void UpdateUniformBuffer(ui32 current_image);

  void Cleanup();
  void CleanupSwapChain();
  [[nodiscard]] VkSurfaceFormatKHR ChooseSurfaceFormat() const;
  [[nodiscard]] VkPresentModeKHR ChoosePresentMode() const;
  [[nodiscard]] VkExtent2D ChooseSwapExtent() const;
  [[nodiscard]] std::vector<const char*> GetRequiredExtensions();
  [[nodiscard]] std::filesystem::path GetContentDir() const noexcept;
  [[nodiscard]] std::filesystem::path GetShadersDir() const noexcept;
  [[nodiscard]] std::filesystem::path GetTexturesDir() const noexcept;
  [[nodiscard]] std::filesystem::path GetModelsDir() const noexcept;

  VkFormat SelectDepthFormat() const;
  [[nodiscard]] VkFormat GetDepthFormat();
  [[nodiscard]] static constexpr VkImageTiling GetDepthImageTiling() noexcept {
    return VK_IMAGE_TILING_OPTIMAL;
  }

  void CreateGpuBufferRaw(const void* data, VkDeviceSize buffer_size,
                          VkBufferUsageFlags usage_flags, VkBuffer& buffer,
                          VkDeviceMemory& buffer_memory);

  template <typename T>
  void CreateGpuBuffer(std::span<const T> view, VkBufferUsageFlags usage_flags,
                       VkBuffer& buffer, VkDeviceMemory& buffer_memory) {
    const VkDeviceSize buffer_size = sizeof(T) * view.size();
    CreateGpuBufferRaw(view.data(), buffer_size, usage_flags, buffer,
                       buffer_memory);
  }

  [[nodiscard]] static TimePoint GetGlobalTime() noexcept {
    return std::chrono::high_resolution_clock::now();
  }
  [[nodiscard]] auto GetTimeSinceAppStart() const noexcept {
    return GetGlobalTime() - app_start_time_;
  }

  void CreateImage(ui32 width, ui32 height, ui32 mip_levels, VkFormat format,
                   VkImageTiling tiling, VkImageUsageFlags usage,
                   VkMemoryPropertyFlags properties, VkImage& image,
                   VkDeviceMemory& image_memory);

  VkImageView CreateImageView(VkImage image, VkFormat format,
                              VkImageAspectFlags aspect_flags, ui32 mip_levels);

 private:
  VkAnnotate annotate_;
  std::filesystem::path executable_file_;
  std::vector<VkImage> swap_chain_images_;
  std::vector<VkImageView> swap_chain_image_views_;
  std::vector<const char*> required_layers_;
  std::vector<const char*> device_extensions_;
  std::vector<VkFramebuffer> swap_chain_frame_buffers_;
  std::vector<VkCommandBuffer> command_buffers_;
  std::vector<VkSemaphore> image_available_semaphores_;
  std::vector<VkSemaphore> render_finished_semaphores_;
  std::vector<VkFence> in_flight_fences_;  // indexed by current frame
  std::vector<VkFence> images_in_flight_;  // indexed by image index
  std::vector<VkBuffer> uniform_buffers_;
  std::vector<VkDeviceMemory> uniform_buffers_memory_;
  std::vector<VkDescriptorSet> descriptor_sets_;
  std::unique_ptr<DeviceSurfaceInfo> surface_info_;
  std::unique_ptr<PhysicalDeviceInfo> device_info_;

  ui32 texture_mip_levels_ = 0;
  VkImage texture_image_ = nullptr;
  VkDeviceMemory texture_image_memory_ = nullptr;
  VkImageView texture_image_view_ = nullptr;
  VkSampler texture_sampler_ = nullptr;

  VkImage depth_image_ = nullptr;
  VkDeviceMemory depth_image_memory_ = nullptr;
  VkImageView depth_image_view_ = nullptr;

  std::vector<Vertex> vertices_;
  std::vector<ui32> indices_;
  VkDeviceMemory vertex_buffer_memory_ = nullptr;
  VkBuffer vertex_buffer_ = nullptr;
  VkDeviceMemory index_buffer_memory_ = nullptr;
  VkBuffer index_buffer_ = nullptr;
  VkQueue graphics_queue_ = nullptr;
  VkQueue present_queue_ = nullptr;
  VkDescriptorPool descriptor_pool_ = nullptr;
  VkCommandPool persistent_command_pool_ = nullptr;
  VkCommandPool transient_command_pool_ = nullptr;
  VkPipeline graphics_pipeline_ = nullptr;
  VkRenderPass render_pass_ = nullptr;
  VkDescriptorSetLayout descriptor_set_layout_ = nullptr;
  VkPipelineLayout pipeline_layout_ = nullptr;
  VkSwapchainKHR swap_chain_ = nullptr;
  VkSurfaceKHR surface_ = nullptr;
  VkDevice device_ = nullptr;
  VkDebugUtilsMessengerEXT debug_messenger_ = nullptr;
  GLFWwindow* window_ = nullptr;
  VkInstance instance_ = nullptr;
  TimePoint app_start_time_;
  size_t current_frame_ = 0;
  std::optional<VkFormat> depth_format_ = {};
  VkExtent2D swap_chain_extent_ = {};
  ui32 window_width_ = kDefaultWindowWidth;
  ui32 window_height_ = kDefaultWindowHeight;
  VkFormat swap_chain_image_format_ = {};
  ui8 glfw_initialized_ : 1;
  ui8 frame_buffer_resized_ : 1;
};