#include "application.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string_view>

#include "fmt/format.h"
#include "image_loader.hpp"
#include "include_glm.hpp"
#include "pipeline/descriptors/vertex_descriptor.hpp"
#include "pipeline/uniform_buffer_object.hpp"
#include "read_file.hpp"
#include "spdlog/spdlog.h"
#include "tiny_obj_loader.h"
#include "unused_var.hpp"
#include "vulkan_utility.hpp"

include_glm_begin;
#include "glm/gtc/matrix_transform.hpp"
include_glm_end;

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* create_info,
    const VkAllocationCallbacks* allocator,
    VkDebugUtilsMessengerEXT* debug_messenger) {
  auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
  if (func != nullptr) {
    return func(instance, create_info, allocator, debug_messenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

static void DestroyDebugUtilsMessengerEXT(
    VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger,
    const VkAllocationCallbacks* allocator) {
  auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
  if (func && debug_messenger) {
    func(instance, debug_messenger, allocator);
  }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
              VkDebugUtilsMessageTypeFlagsEXT message_type,
              const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
              void* user_data) {
  UnusedVar(user_data, message_type);

  switch (severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
      spdlog::trace("validation layer: {}\n", callback_data->pMessage);
      break;

    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
      spdlog::info("validation layer: {}\n", callback_data->pMessage);
      break;

    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
      spdlog::warn("validation layer: {}\n", callback_data->pMessage);
      break;

    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
      spdlog::error("validation layer: {}\n", callback_data->pMessage);
      break;

    default:
      assert(false);
      break;
  }

  return kVkFalse;
}

Application::Application() {
  spdlog::set_level(spdlog::level::trace);

  glfw_initialized_ = false;
  frame_buffer_resized_ = false;

  if constexpr (kEnableValidation) {
    required_layers_.push_back("VK_LAYER_KHRONOS_validation");
  }

  if constexpr (kEnableOverlay) {
    required_layers_.push_back("VK_LAYER_MESA_overlay");
  }

  if constexpr (kEnableLunarGMonitor) {
    required_layers_.push_back("VK_LAYER_LUNARG_monitor");
  }

  device_extensions_.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  app_start_time_ = GetGlobalTime();
}

Application::~Application() { Cleanup(); }

void Application::SetExecutableFile(std::filesystem::path path) {
  executable_file_ = path;
}

void Application::Run() {
  InitializeWindow();
  InitializeVulkan();
  MainLoop();
  Cleanup();
}

void Application::InitializeWindow() {
  glfwInit();
  glfw_initialized_ = true;
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  window_ = glfwCreateWindow(static_cast<int>(window_width_),
                             static_cast<int>(window_height_), "Vulkan",
                             nullptr, nullptr);
  glfwSetWindowUserPointer(window_, this);
  glfwSetFramebufferSizeCallback(window_,
                                 Application::FrameBufferResizeCallback);
}

void Application::FrameBufferResizeCallback(GLFWwindow* window, int width,
                                            int height) {
  UnusedVar(width, height);
  auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
  app->frame_buffer_resized_ = true;
}

void populate_debug_messenger_create_info(
    VkDebugUtilsMessengerCreateInfoEXT& create_info) {
  create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  create_info.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  create_info.pfnUserCallback = DebugCallback;
  create_info.pUserData = nullptr;
}

void Application::PickPhysicalDevice() {
  std::vector<VkPhysicalDevice> devices;
  VulkanUtility::GetDevices(instance_, devices);

  [[unlikely]] if (devices.empty()) {
    throw std::runtime_error("There is no vulkan capable devices");
  }

  device_info_ = std::make_unique<PhysicalDeviceInfo>();
  surface_info_ = std::make_unique<DeviceSurfaceInfo>();

  int best_score = -1;
  for (size_t i = 0; i < devices.size(); ++i) {
    PhysicalDeviceInfo device_info;
    device_info.Populate(devices[i], surface_);

    bool has_extensions = true;
    for (auto& required_extension : device_extensions_) {
      if (!device_info.HasExtension(required_extension)) {
        has_extensions = false;
        break;
      }
    }

    if (!has_extensions) {
      continue;
    }

    // Check swapchain compatibility
    DeviceSurfaceInfo surface_info;
    surface_info.Populate(devices[i], surface_);
    if (surface_info.formats.empty() || surface_info.present_modes.empty()) {
      continue;
    }

    const int score = device_info.RateDevice();
    if (score > best_score) {
      best_score = score;
      *device_info_ = std::move(device_info);
      *surface_info_ = std::move(surface_info);
    }
  }

  [[unlikely]] if (best_score < 0) {
    throw std::runtime_error("There is no suitable device");
  }

  msaa_samples_ = device_info_->GetMaxUsableSampleCount();

  spdlog::info("picked physical device:");
  spdlog::info("   name: {}", device_info_->properties.deviceName);
  spdlog::info(
      "   sampler anisotropy: {} {}",
      device_info_->features.samplerAnisotropy ? "enabled" : "disabled",
      device_info_->properties.limits.maxSamplerAnisotropy);
  spdlog::info("   MSAA max samples: {}",
               VulkanUtility::SampleCountFlagsToString(msaa_samples_));
}

void Application::CreateSurface() {
  VkWrap(glfwCreateWindowSurface)(instance_, window_, nullptr, &surface_);
}

void Application::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                               VkMemoryPropertyFlags properties,
                               VkBuffer& buffer,
                               VkDeviceMemory& buffer_memory) const {
  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = size;
  buffer_info.usage = usage;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VkWrap(vkCreateBuffer)(device_, &buffer_info, nullptr, &buffer);

  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(device_, buffer, &memory_requirements);

  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = memory_requirements.size;
  alloc_info.memoryTypeIndex = device_info_->GetMemoryTypeIndex(
      memory_requirements.memoryTypeBits, properties);

  VkWrap(vkAllocateMemory)(device_, &alloc_info, nullptr, &buffer_memory);
  VkWrap(vkBindBufferMemory)(device_, buffer, buffer_memory, 0u);
}

void Application::CreateDevice() {
  float queue_priority = 1.0f;

  std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

  auto add_queue_family = [&](ui32 idx) {
    if (std::find_if(queue_create_infos.begin(), queue_create_infos.end(),
                     [idx](auto& info) {
                       return info.queueFamilyIndex == idx;
                     }) == queue_create_infos.end()) {
      VkDeviceQueueCreateInfo queue_create_info{};
      queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queue_create_info.queueFamilyIndex = idx;
      queue_create_info.queueCount = 1;
      queue_create_info.pQueuePriorities = &queue_priority;
      queue_create_infos.push_back(queue_create_info);
    }
  };

  add_queue_family(device_info_->GetGraphicsQueueFamilyIndex());
  add_queue_family(device_info_->GetPresentQueueFamilyIndex());

  VkPhysicalDeviceFeatures device_features{};
  device_features.samplerAnisotropy = device_info_->features.samplerAnisotropy;
  device_features.sampleRateShading =
      kVkTrue;  // enable sample shading freature for the device

  VkDeviceCreateInfo device_create_info{};
  device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create_info.pQueueCreateInfos = queue_create_infos.data();
  device_create_info.queueCreateInfoCount =
      static_cast<ui32>(queue_create_infos.size());
  device_create_info.pEnabledFeatures = &device_features;
  device_create_info.enabledLayerCount =
      0;  // need to specify them in instance only
  device_create_info.enabledExtensionCount =
      static_cast<ui32>(device_extensions_.size());

  if (!device_extensions_.empty()) {
    device_create_info.ppEnabledExtensionNames = device_extensions_.data();
  }

  VkDevice logical_device;
  VkWrap(vkCreateDevice)(device_info_->device, &device_create_info, nullptr,
                         &logical_device);

  device_ = logical_device;
  vkGetDeviceQueue(device_, device_info_->GetGraphicsQueueFamilyIndex(), 0,
                   &graphics_queue_);
  vkGetDeviceQueue(device_, device_info_->GetPresentQueueFamilyIndex(), 0,
                   &present_queue_);
}

void Application::CreateSwapChain() {
  surface_info_->Populate(device_info_->device, surface_);
  const VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat();
  const VkPresentModeKHR presentMode = ChoosePresentMode();
  swap_chain_extent_ = ChooseSwapExtent();

  ui32 image_count = surface_info_->capabilities.minImageCount + 1;
  if (surface_info_->capabilities.maxImageCount > 0 &&
      image_count > surface_info_->capabilities.maxImageCount) {
    image_count = surface_info_->capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = surface_;
  create_info.minImageCount = image_count;
  create_info.imageFormat = surfaceFormat.format;
  create_info.imageColorSpace = surfaceFormat.colorSpace;
  create_info.imageExtent = swap_chain_extent_;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  std::array queue_family_indices = {
      device_info_->GetGraphicsQueueFamilyIndex(),
      device_info_->GetPresentQueueFamilyIndex()};

  if (queue_family_indices[0] != queue_family_indices[1]) {
    create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount =
        static_cast<ui32>(queue_family_indices.size());
    create_info.pQueueFamilyIndices = queue_family_indices.data();
  } else {
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;      // Optional
    create_info.pQueueFamilyIndices = nullptr;  // Optional
  }

  create_info.preTransform = surface_info_->capabilities.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = presentMode;
  create_info.clipped = kVkTrue;
  create_info.oldSwapchain = nullptr;

  VkWrap(vkCreateSwapchainKHR)(device_, &create_info, nullptr, &swap_chain_);

  VulkanUtility::GetSwapChainImages(device_, swap_chain_, swap_chain_images_);
  swap_chain_image_format_ = surfaceFormat.format;
}

void Application::CreateSwapChainImageViews() {
  const size_t num_images = swap_chain_images_.size();
  swap_chain_image_views_.resize(num_images);
  constexpr ui32 mip_levels = 1;
  for (size_t i = 0; i != num_images; i++) {
    swap_chain_image_views_[i] =
        CreateImageView(swap_chain_images_[i], swap_chain_image_format_,
                        VK_IMAGE_ASPECT_COLOR_BIT, mip_levels);
  }
}

void Application::CreateRenderPass() {
  VkAttachmentDescription color_attachment{};
  color_attachment.format = swap_chain_image_format_;
  color_attachment.samples = msaa_samples_;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference color_attachment_ref{};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription depth_attachment{};
  depth_attachment.format = GetDepthFormat();
  depth_attachment.samples = msaa_samples_;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_attachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_attachment_ref{};
  depth_attachment_ref.attachment = 1;
  depth_attachment_ref.layout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription color_attachment_resolve{};
  color_attachment_resolve.format = swap_chain_image_format_;
  color_attachment_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment_resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment_resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_attachment_resolve_ref{};
  color_attachment_resolve_ref.attachment = 2;
  color_attachment_resolve_ref.layout =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;
  subpass.pDepthStencilAttachment = &depth_attachment_ref;
  subpass.pResolveAttachments = &color_attachment_resolve_ref;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcAccessMask = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  const std::array attachments{color_attachment, depth_attachment,
                               color_attachment_resolve};

  VkRenderPassCreateInfo render_pass_create_info{};
  render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_create_info.attachmentCount =
      static_cast<ui32>(attachments.size());
  render_pass_create_info.pAttachments = attachments.data();
  render_pass_create_info.subpassCount = 1;
  render_pass_create_info.pSubpasses = &subpass;
  render_pass_create_info.dependencyCount = 1;
  render_pass_create_info.pDependencies = &dependency;

  VkWrap(vkCreateRenderPass)(device_, &render_pass_create_info, nullptr,
                             &render_pass_);
}

void Application::CreateDescriptorSetLayout() {
  VkDescriptorSetLayoutBinding ubo_layout_binding{};
  ubo_layout_binding.binding = 0;
  ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  ubo_layout_binding.descriptorCount = 1;
  ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  ubo_layout_binding.pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutBinding sampler_layout_binding{};
  sampler_layout_binding.binding = 1;
  sampler_layout_binding.descriptorCount = 1;
  sampler_layout_binding.descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  sampler_layout_binding.pImmutableSamplers = nullptr;

  const std::array bindings{ubo_layout_binding, sampler_layout_binding};

  VkDescriptorSetLayoutCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  create_info.bindingCount = static_cast<ui32>(bindings.size());
  create_info.pBindings = bindings.data();

  VkWrap(vkCreateDescriptorSetLayout)(device_, &create_info, nullptr,
                                      &descriptor_set_layout_);
}

void Application::CreateGraphicsPipeline() {
  const auto shaders_dir = GetShadersDir();

  std::vector<char> cache;
  VkShaderModule vert_shader_module =
      CreateShaderModule(shaders_dir / "vertex_shader.spv", cache);
  VkShaderModule fragment_shader_module =
      CreateShaderModule(shaders_dir / "fragment_shader.spv", cache);

  VkPipelineShaderStageCreateInfo vert_shader_stage_create_info{};
  vert_shader_stage_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vert_shader_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vert_shader_stage_create_info.module = vert_shader_module;
  vert_shader_stage_create_info.pName = "main";

  VkPipelineShaderStageCreateInfo frag_shader_stage_create_info{};
  frag_shader_stage_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  frag_shader_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  frag_shader_stage_create_info.module = fragment_shader_module;
  frag_shader_stage_create_info.pName = "main";

  auto binding_descriptions = StructDescriptor<Vertex>::GetBindingDescription();
  auto attribute_descriptions =
      StructDescriptor<Vertex>::GetInputAttributeDescriptions();

  VkPipelineVertexInputStateCreateInfo vert_input_info{};
  vert_input_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vert_input_info.vertexBindingDescriptionCount =
      static_cast<ui32>(binding_descriptions.size());
  vert_input_info.pVertexBindingDescriptions = binding_descriptions.data();
  vert_input_info.vertexAttributeDescriptionCount =
      static_cast<ui32>(attribute_descriptions.size());
  vert_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = kVkFalse;

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(swap_chain_extent_.width);
  viewport.height = static_cast<float>(swap_chain_extent_.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = swap_chain_extent_;

  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.pViewports = &viewport;
  viewport_state.scissorCount = 1;
  viewport_state.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = kVkFalse;
  rasterizer.rasterizerDiscardEnable = kVkFalse;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = kVkFalse;
  rasterizer.depthBiasConstantFactor = 0.0f;  // Optional
  rasterizer.depthBiasClamp = 0.0f;           // Optional
  rasterizer.depthBiasSlopeFactor = 0.0f;     // Optional

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = kVkTrue;
  multisampling.rasterizationSamples = msaa_samples_;
  multisampling.minSampleShading = 0.2f;           // Optional
  multisampling.pSampleMask = nullptr;             // Optional
  multisampling.alphaToCoverageEnable = kVkFalse;  // Optional
  multisampling.alphaToOneEnable = kVkFalse;       // Optional

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = kVkFalse;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;   // Optional
  colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;  // Optional
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;              // Optional
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;   // Optional
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  // Optional
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;              // Optional

  VkPipelineDepthStencilStateCreateInfo depth_stencil{};
  depth_stencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.depthTestEnable = kVkTrue;
  depth_stencil.depthWriteEnable = kVkTrue;
  depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depth_stencil.depthBoundsTestEnable = kVkFalse;
  depth_stencil.minDepthBounds = 0.0f;
  depth_stencil.maxDepthBounds = 1.0f;
  depth_stencil.stencilTestEnable = kVkFalse;
  depth_stencil.front = {};
  depth_stencil.back = {};

  VkPipelineColorBlendStateCreateInfo color_blending{};
  color_blending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.logicOpEnable = kVkFalse;
  color_blending.logicOp = VK_LOGIC_OP_COPY;  // Optional
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &colorBlendAttachment;
  color_blending.blendConstants[0] = 0.0f;  // Optional
  color_blending.blendConstants[1] = 0.0f;  // Optional
  color_blending.blendConstants[2] = 0.0f;  // Optional
  color_blending.blendConstants[3] = 0.0f;  // Optional

  VkPipelineLayoutCreateInfo pipline_layout_info{};
  pipline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipline_layout_info.setLayoutCount = 1;
  pipline_layout_info.pSetLayouts = &descriptor_set_layout_;
  pipline_layout_info.pushConstantRangeCount = 0;     // Optional
  pipline_layout_info.pPushConstantRanges = nullptr;  // Optional
  VkWrap(vkCreatePipelineLayout)(device_, &pipline_layout_info, nullptr,
                                 &pipeline_layout_);

  std::array shader_stages{vert_shader_stage_create_info,
                           frag_shader_stage_create_info};
  VkGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = static_cast<ui32>(shader_stages.size());
  pipeline_info.pStages = shader_stages.data();
  pipeline_info.pVertexInputState = &vert_input_info;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pDepthStencilState = &depth_stencil;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDynamicState = nullptr;  // Optional
  pipeline_info.layout = pipeline_layout_;
  pipeline_info.renderPass = render_pass_;
  pipeline_info.subpass = 0;
  pipeline_info.basePipelineHandle = nullptr;  // Optional
  pipeline_info.basePipelineIndex = -1;        // Optional

  VkWrap(vkCreateGraphicsPipelines)(device_, nullptr, 1u, &pipeline_info,
                                    nullptr, &graphics_pipeline_);

  VulkanUtility::Destroy<vkDestroyShaderModule>(device_, vert_shader_module);
  VulkanUtility::Destroy<vkDestroyShaderModule>(device_,
                                                fragment_shader_module);
}

void Application::CreateFrameBuffers() {
  const size_t num_images = static_cast<ui32>(swap_chain_image_views_.size());
  swap_chain_frame_buffers_.resize(num_images);

  for (size_t index = 0; index < num_images; ++index) {
    std::array attachments{color_image_view_, depth_image_view_,
                           swap_chain_image_views_[index]};

    VkFramebufferCreateInfo frame_buffer_info{};
    frame_buffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frame_buffer_info.renderPass = render_pass_;
    frame_buffer_info.attachmentCount = static_cast<ui32>(attachments.size());
    frame_buffer_info.pAttachments = attachments.data();
    frame_buffer_info.width = swap_chain_extent_.width;
    frame_buffer_info.height = swap_chain_extent_.height;
    frame_buffer_info.layers = 1;

    VkWrap(vkCreateFramebuffer)(device_, &frame_buffer_info, nullptr,
                                &swap_chain_frame_buffers_[index]);
  }
}

VkCommandPool Application::CreateCommandPool(
    ui32 queue_family_index, VkCommandPoolCreateFlags flags) const {
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.queueFamilyIndex = queue_family_index;
  pool_info.flags = flags;

  VkCommandPool pool;
  VkWrap(vkCreateCommandPool)(device_, &pool_info, nullptr, &pool);
  return pool;
}

void Application::CreateCommandPools() {
  persistent_command_pool_ =
      CreateCommandPool(device_info_->GetGraphicsQueueFamilyIndex());
  transient_command_pool_ =
      CreateCommandPool(device_info_->GetGraphicsQueueFamilyIndex(),
                        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
}

void Application::CreateImage(ui32 width, ui32 height, ui32 mip_levels,
                              VkSampleCountFlagBits samples, VkFormat format,
                              VkImageTiling tiling, VkImageUsageFlags usage,
                              VkMemoryPropertyFlags properties, VkImage& image,
                              VkDeviceMemory& image_memory) {
  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent.width = width;
  image_info.extent.height = height;
  image_info.extent.depth = 1;
  image_info.mipLevels = mip_levels;
  image_info.arrayLayers = 1;
  image_info.format = format;
  image_info.tiling = tiling;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage = usage;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.samples = samples;
  image_info.flags = 0;
  VkWrap(vkCreateImage)(device_, &image_info, nullptr, &image);

  VkMemoryRequirements memory_requirements{};
  vkGetImageMemoryRequirements(device_, image, &memory_requirements);

  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = memory_requirements.size;
  alloc_info.memoryTypeIndex = device_info_->GetMemoryTypeIndex(
      memory_requirements.memoryTypeBits, properties);
  VkWrap(vkAllocateMemory)(device_, &alloc_info, nullptr, &image_memory);
  VkWrap(vkBindImageMemory)(device_, image, image_memory, 0u);
}

void Application::CreateTextureImages() {
  constexpr VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;
  std::filesystem::path texture_path = GetTexturesDir() / "viking_room.png";

  // check that linear sampling is supported for this image format
  // this is required for mipmaps generation
  if (!(device_info_->GetFormatProperties(image_format).optimalTilingFeatures &
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
    throw std::runtime_error(
        "VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT is not supporte for "
        "image format");
  }

  // read texture from file, create image and device memory
  {
    VkBuffer staging_buffer = nullptr;
    VkDeviceMemory staging_buffer_memory = nullptr;

    const ImageLoader image(texture_path.string());
    texture_mip_levels_ = 1 + static_cast<ui32>(std::floor(std::log2(std::max(
                                  image.GetWidth(), image.GetHeight()))));
    const auto image_data = image.GetData();
    CreateBuffer(image_data.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 staging_buffer, staging_buffer_memory);

    VulkanUtility::MapCopyUnmap(image_data.data(), image_data.size(), device_,
                                staging_buffer_memory);

    CreateImage(image.GetWidth(), image.GetHeight(), texture_mip_levels_,
                VK_SAMPLE_COUNT_1_BIT, image_format, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture_image_,
                texture_image_memory_);

    annotate_.SetObjectName(device_, texture_image_, "texture image");
    annotate_.SetObjectName(device_, texture_image_memory_, "texture memory");

    ExecuteSingleTimeCommands([&](VkCommandBuffer command_buffer) {
      TransitionImageLayout(command_buffer, texture_image_, image_format,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            texture_mip_levels_);
      CopyBufferToImage(command_buffer, staging_buffer, texture_image_,
                        image.GetWidth(), image.GetHeight());
      // transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while
      // generating mips
      GenerateMipMaps_Blit(command_buffer, texture_image_, image.GetWidth(),
                           image.GetHeight(), texture_mip_levels_);
    });

    VulkanUtility::Destroy<vkDestroyBuffer>(device_, staging_buffer);
    VulkanUtility::FreeMemory(device_, staging_buffer_memory);
  }

  // create image view
  texture_image_view_ =
      CreateImageView(texture_image_, image_format, VK_IMAGE_ASPECT_COLOR_BIT,
                      texture_mip_levels_);
  annotate_.SetObjectName(
      device_, texture_image_view_,
      fmt::format("image view for texture {}", texture_path.stem().string()));

  // create texture sampler
  {
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;  // oversampling
    sampler_info.minFilter = VK_FILTER_LINEAR;  // undersampling
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = device_info_->features.samplerAnisotropy;
    sampler_info.maxAnisotropy =
        device_info_->properties.limits.maxSamplerAnisotropy;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = kVkFalse;
    sampler_info.compareEnable = kVkFalse;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = static_cast<float>(texture_mip_levels_);
    VkWrap(vkCreateSampler)(device_, &sampler_info, nullptr, &texture_sampler_);
    annotate_.SetObjectName(device_, texture_sampler_, "texture sampler");
  }
}

void Application::CreateDepthResources() {
  const VkFormat format = GetDepthFormat();
  const VkImageTiling tiling = GetDepthImageTiling();
  constexpr ui32 mip_levels = 1;
  CreateImage(swap_chain_extent_.width, swap_chain_extent_.height, mip_levels,
              msaa_samples_, format, tiling,
              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depth_image_,
              depth_image_memory_);

  annotate_.SetObjectName(device_, depth_image_, "depth image");
  annotate_.SetObjectName(device_, depth_image_memory_, "depth image memory");

  depth_image_view_ = CreateImageView(depth_image_, format,
                                      VK_IMAGE_ASPECT_DEPTH_BIT, mip_levels);

  annotate_.SetObjectName(device_, depth_image_view_, "depth image view");

  ExecuteSingleTimeCommands([&](VkCommandBuffer command_buffer) {
    TransitionImageLayout(
        command_buffer, depth_image_, format, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, mip_levels);
  });
}

void Application::CreateColorResources() {
  const VkFormat color_format = swap_chain_image_format_;
  const ui32 mip_levels = 1;
  CreateImage(swap_chain_extent_.width, swap_chain_extent_.height, mip_levels,
              msaa_samples_, color_format, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, color_image_,
              color_image_memory_);
  annotate_.SetObjectName(device_, color_image_, "color image");
  annotate_.SetObjectName(device_, color_image_memory_, "color image memory");
  color_image_view_ =
      CreateImageView(color_image_, color_format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
  annotate_.SetObjectName(device_, color_image_view_, "color image view");
}

VkCommandBuffer Application::BeginSingleTimeCommands() {
  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandPool = transient_command_pool_;
  alloc_info.commandBufferCount = 1;

  VkCommandBuffer command_buffer{};
  VkWrap(vkAllocateCommandBuffers)(device_, &alloc_info, &command_buffer);

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VkWrap(vkBeginCommandBuffer)(command_buffer, &begin_info);

  return command_buffer;
}

void Application::EndSingleTimeCommands(VkCommandBuffer command_buffer) {
  VkWrap(vkEndCommandBuffer)(command_buffer);
  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;

  VkWrap(vkQueueSubmit)(graphics_queue_, 1u, &submit_info, nullptr);
  VkWrap(vkQueueWaitIdle)(graphics_queue_);

  vkFreeCommandBuffers(device_, transient_command_pool_, 1u, &command_buffer);
}

namespace std {
template <>
struct hash<tinyobj::index_t> {
  size_t operator()(const tinyobj::index_t& index) const noexcept {
    return (hash<int>()(index.vertex_index) ^
            hash<int>()(index.texcoord_index) ^
            hash<int>()(index.normal_index));
  }
};
}  // namespace std

namespace tinyobj {
bool operator==(const index_t& a, const index_t& b) noexcept {
  return a.vertex_index == b.vertex_index &&
         a.texcoord_index == b.texcoord_index &&
         a.normal_index == b.normal_index;
}
}  // namespace tinyobj

void Application::LoadModel() {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  const auto model_path = (GetModelsDir() / "viking_room.obj").string();

  [[unlikely]] if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                                     model_path.data())) {
    throw std::runtime_error(warn + err);
  }

  std::unordered_map<tinyobj::index_t, ui32> index_remap;
  for (const tinyobj::shape_t& shape : shapes) {
    indices_.reserve(indices_.size() + shape.mesh.indices.size());
    for (const tinyobj::index_t& model_index : shape.mesh.indices) {
      auto map_iterator = index_remap.find(model_index);
      if (map_iterator == index_remap.end()) {
        Vertex v{};
        const size_t vertex_offset =
            static_cast<size_t>(3 * model_index.vertex_index);
        v.pos = {attrib.vertices[vertex_offset + 0u],
                 attrib.vertices[vertex_offset + 1u],
                 attrib.vertices[vertex_offset + 2u]};

        const size_t texcoord_offset =
            static_cast<size_t>(2 * model_index.texcoord_index);
        v.tex_coord = {attrib.texcoords[texcoord_offset + 0u],
                       1.0f - attrib.texcoords[texcoord_offset + 1u]};
        v.color = {1.0f, 1.0f, 1.0f};
        const ui32 index = static_cast<ui32>(vertices_.size());
        vertices_.push_back(v);

        auto [it, inserted] = index_remap.insert({model_index, index});
        map_iterator = it;
      }

      indices_.push_back(map_iterator->second);
    }
  }
}

void Application::CreateVertexBuffers() {
  CreateGpuBuffer(std::span<const Vertex>(vertices_),
                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertex_buffer_,
                  vertex_buffer_memory_);
}

void Application::CreateIndexBuffers() {
  CreateGpuBuffer(std::span<const ui32>(indices_),
                  VK_BUFFER_USAGE_INDEX_BUFFER_BIT, index_buffer_,
                  index_buffer_memory_);
}

void Application::CreateUniformBuffers() {
  VkDeviceSize buffer_size = sizeof(UniformBufferObject);

  const VkBufferUsageFlags buffer_usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  const VkMemoryPropertyFlags buffer_flags =
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

  const size_t num_buffers = swap_chain_images_.size();
  uniform_buffers_.resize(num_buffers);
  uniform_buffers_memory_.resize(num_buffers);
  for (size_t i = 0; i < num_buffers; ++i) {
    CreateBuffer(buffer_size, buffer_usage, buffer_flags, uniform_buffers_[i],
                 uniform_buffers_memory_[i]);
  }
}

void Application::CreateDescriptorPool() {
  std::array<VkDescriptorPoolSize, 2> pool_sizes{};
  pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_sizes[0].descriptorCount = static_cast<ui32>(swap_chain_images_.size());
  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  pool_sizes[1].descriptorCount = static_cast<ui32>(swap_chain_images_.size());

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = static_cast<ui32>(pool_sizes.size());
  pool_info.pPoolSizes = pool_sizes.data();
  pool_info.maxSets = static_cast<ui32>(swap_chain_images_.size());

  VkWrap(vkCreateDescriptorPool)(device_, &pool_info, nullptr,
                                 &descriptor_pool_);
}

void Application::CreateDescriptorSets() {
  const ui32 num_descriptor_sets = static_cast<ui32>(swap_chain_images_.size());
  std::vector<VkDescriptorSetLayout> layouts(num_descriptor_sets,
                                             descriptor_set_layout_);

  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = descriptor_pool_;
  alloc_info.descriptorSetCount = num_descriptor_sets;
  alloc_info.pSetLayouts = layouts.data();

  descriptor_sets_.resize(num_descriptor_sets);
  VkWrap(vkAllocateDescriptorSets)(device_, &alloc_info,
                                   descriptor_sets_.data());

  for (ui32 i = 0; i < num_descriptor_sets; ++i) {
    std::array<VkWriteDescriptorSet, 2> wds{};

    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = uniform_buffers_[i];
    buffer_info.offset = 0;
    buffer_info.range = sizeof(UniformBufferObject);
    wds[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wds[0].dstSet = descriptor_sets_[i];
    wds[0].dstBinding = 0;
    wds[0].dstArrayElement = 0;
    wds[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    wds[0].descriptorCount = 1;
    wds[0].pBufferInfo = &buffer_info;

    VkDescriptorImageInfo image_info{};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = texture_image_view_;
    image_info.sampler = texture_sampler_;
    wds[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wds[1].dstSet = descriptor_sets_[i];
    wds[1].dstBinding = 1;
    wds[1].dstArrayElement = 0;
    wds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wds[1].descriptorCount = 1;
    wds[1].pImageInfo = &image_info;

    vkUpdateDescriptorSets(device_, static_cast<ui32>(wds.size()), wds.data(),
                           0, nullptr);
  }
}

void Application::CopyBuffer(VkCommandBuffer command_buffer, VkBuffer src,
                             VkBuffer dst, VkDeviceSize size) {
  VkBufferCopy copy_region{};
  copy_region.size = size;
  vkCmdCopyBuffer(command_buffer, src, dst, 1, &copy_region);
}

void Application::CopyBufferToImage(VkCommandBuffer command_buffer,
                                    VkBuffer src, VkImage image, ui32 width,
                                    ui32 height) {
  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;

  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;

  region.imageOffset = {0, 0, 0};
  region.imageExtent = {width, height, 1};

  vkCmdCopyBufferToImage(command_buffer, src, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void Application::TransitionImageLayout(VkCommandBuffer command_buffer,
                                        VkImage image, VkFormat format,
                                        VkImageLayout old_layout,
                                        VkImageLayout new_layout,
                                        ui32 mip_levels) {
  UnusedVar(format);
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = mip_levels;
  barrier.subresourceRange.layerCount = 1;

  if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (VulkanUtility::FormatHasStencilComponent(format)) {
      barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  } else {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  }

  VkPipelineStageFlags source_stage;
  VkPipelineStageFlags destination_stage;

  // undefined -> transfer destination optimal
  if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
      new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  }
  // undefined -> depth stencil attachment optimal
  else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
           new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  }
  // transfer destination optimal -> shader read only optimal
  else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
           new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    throw std::invalid_argument("unsupported layout transition");
  }

  vkCmdPipelineBarrier(
      command_buffer,
      source_stage,  // in which pipeline stage the operations occur that should
                     // happen before barrier
      destination_stage,  // the pipeline stage in which operations will wait on
                          // the barrier
      0,                  // 0 or VK_DEPENDENCY_BY_REGION_BIT
      0, nullptr,         // memory barriers
      0, nullptr,         // buffer memory barriers
      1, &barrier);       // image memory barriers
}

void Application::GenerateMipMaps_Blit(VkCommandBuffer command_buffer,
                                       VkImage image, ui32 width, ui32 height,
                                       ui32 mip_levels) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.image = image;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  i32 mip_width = static_cast<i32>(width);
  i32 mip_height = static_cast<i32>(height);
  for (ui32 mip_level = 1; mip_level < mip_levels; ++mip_level) {
    // Transfer mip - 1 level from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to
    // VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL this transition will wait for the
    // previous mip level to be filled, either from the previous blit command,
    // or from vkCmdCopyBufferToImage. The current blit command will wait on
    // this transition
    barrier.subresourceRange.baseMipLevel = mip_level - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    VkImageBlit blit{};
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {mip_width, mip_height, 1};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = mip_level - 1;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1].x = std::max(mip_width / 2, 1);
    blit.dstOffsets[1].y = std::max(mip_height / 2, 1);
    blit.dstOffsets[1].z = 1;
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = mip_level;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;
    vkCmdBlitImage(command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_LINEAR);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);
    mip_width = std::max(mip_width / 2, 1);
    mip_height = std::max(mip_height / 2, 1);
  }

  barrier.subresourceRange.baseMipLevel = mip_levels - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
}

void Application::CreateCommandBuffers() {
  ui32 num_buffers = static_cast<ui32>(swap_chain_frame_buffers_.size());
  command_buffers_.resize(num_buffers);

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = persistent_command_pool_;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = num_buffers;
  VkWrap(vkAllocateCommandBuffers)(device_, &allocInfo,
                                   command_buffers_.data());

  for (ui32 i = 0; i != num_buffers; ++i) {
    VkCommandBuffer command_buffer = command_buffers_[i];

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;                   // Optional
    beginInfo.pInheritanceInfo = nullptr;  // Optional

    VkWrap(vkBeginCommandBuffer)(command_buffer, &beginInfo);
    std::array<VkClearValue, 2> clear_values{};
    clear_values[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clear_values[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass_;
    render_pass_info.framebuffer = swap_chain_frame_buffers_[i];
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = swap_chain_extent_;
    render_pass_info.clearValueCount = static_cast<ui32>(clear_values.size());
    render_pass_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(command_buffer, &render_pass_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    auto draw_frame_label = annotate_.ScopedLabel(command_buffer, "draw frame",
                                                  LabelColor::Green());
    {
      vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        graphics_pipeline_);

      std::array vertex_buffers{vertex_buffer_};
      const ui32 num_vertex_buffers = static_cast<ui32>(vertex_buffers.size());
      std::array offsets{VkDeviceSize(0)};
      vkCmdBindVertexBuffers(command_buffer, 0, num_vertex_buffers,
                             vertex_buffers.data(), offsets.data());
      vkCmdBindIndexBuffer(command_buffer, index_buffer_, 0,
                           VK_INDEX_TYPE_UINT32);
      vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline_layout_, 0, 1, &descriptor_sets_[i], 0,
                              nullptr);

      ui32 num_instances = 1;
      const ui32 num_indices = static_cast<ui32>(indices_.size());
      vkCmdDrawIndexed(command_buffer, num_indices, num_instances, 0, 0, 0);
    }

    vkCmdEndRenderPass(command_buffer);

    VkWrap(vkEndCommandBuffer)(command_buffer);
  }
}

VkShaderModule Application::CreateShaderModule(
    const std::filesystem::path& file, std::vector<char>& shader_code) {
  ReadFile(file, shader_code);

  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = shader_code.size();
  create_info.pCode = reinterpret_cast<const uint32_t*>(shader_code.data());
  VkShaderModule shader_module;
  VkWrap(vkCreateShaderModule)(device_, &create_info, nullptr, &shader_module);

  return shader_module;
}

void Application::CheckRequiredLayersSupport() {
  if (!required_layers_.empty()) {
    std::vector<VkLayerProperties> available_layers;
    VulkanUtility::GetInstanceLayerProperties(available_layers);

    for (const auto& layer_name : required_layers_) {
      std::string_view name_view(layer_name);
      const auto it =
          std::find_if(available_layers.begin(), available_layers.end(),
                       [&](const VkLayerProperties& layer_properties) {
                         return name_view == layer_properties.layerName;
                       });

      [[unlikely]] if (it == available_layers.end()) {
        auto message = fmt::format("{} layer is not present", name_view);
        throw std::runtime_error(std::move(message));
      }
    }
  }
}

void Application::InitializeVulkan() {
  CreateInstance();
  annotate_.Initialize(instance_);
  SetupDebugMessenger();
  CreateSurface();
  PickPhysicalDevice();
  CreateDevice();
  CreateSwapChain();
  CreateSwapChainImageViews();
  CreateRenderPass();
  CreateDescriptorSetLayout();
  CreateGraphicsPipeline();
  CreateCommandPools();
  CreateTextureImages();
  CreateColorResources();
  CreateDepthResources();
  CreateFrameBuffers();
  LoadModel();
  CreateVertexBuffers();
  CreateIndexBuffers();
  CreateUniformBuffers();
  CreateDescriptorPool();
  CreateDescriptorSets();
  CreateCommandBuffers();
  CreateSyncObjects();
}

void Application::RecreateSwapChain() {
  int width = 0, height = 0;
  glfwGetFramebufferSize(window_, &width, &height);
  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(window_, &width, &height);
    glfwWaitEvents();
  }

  if (device_) {
    VkWrap(vkDeviceWaitIdle)(device_);
  }

  CleanupSwapChain();

  CreateSwapChain();
  CreateSwapChainImageViews();
  CreateRenderPass();
  CreateGraphicsPipeline();
  CreateColorResources();
  CreateDepthResources();
  CreateFrameBuffers();
  CreateUniformBuffers();
  CreateDescriptorPool();
  CreateDescriptorSets();
  CreateCommandBuffers();
}

void Application::CreateInstance() {
  CheckRequiredLayersSupport();

  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Hello Triangle";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "No Engine";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_0;

  auto required_extensions = GetRequiredExtensions();

  VkInstanceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledExtensionCount =
      static_cast<ui32>(required_extensions.size());
  create_info.ppEnabledExtensionNames = required_extensions.data();
  create_info.enabledLayerCount = static_cast<ui32>(required_layers_.size());

  VkDebugUtilsMessengerCreateInfoEXT create_messenger_info{};
  if (create_info.enabledLayerCount > 0) {
    create_info.ppEnabledLayerNames = required_layers_.data();
    populate_debug_messenger_create_info(create_messenger_info);
    create_info.pNext = &create_messenger_info;
  }

  VkWrap(vkCreateInstance)(&create_info, nullptr, &instance_);
}

void Application::SetupDebugMessenger() {
  if constexpr (kEnableDebugMessengerExtension) {
    VkDebugUtilsMessengerCreateInfoEXT create_info{};
    populate_debug_messenger_create_info(create_info);
    VkWrap(CreateDebugUtilsMessengerEXT)(instance_, &create_info, nullptr,
                                         &debug_messenger_);
  }
}

void Application::MainLoop() {
  while (!glfwWindowShouldClose(window_)) {
    glfwPollEvents();
    DrawFrame();
  }
}

void Application::DrawFrame() {
  // first check that nobody does not draw to current frame
  VkWrap(vkWaitForFences)(device_, 1u, &in_flight_fences_[current_frame_],
                          kVkTrue, UINT64_MAX);

  // get next image index from the swap chain
  ui32 image_index;

  {
    const VkResult acquire_result = vkAcquireNextImageKHR(
        device_, swap_chain_, UINT64_MAX,
        image_available_semaphores_[current_frame_], nullptr, &image_index);

    switch (acquire_result) {
      case VK_SUCCESS:
      case VK_SUBOPTIMAL_KHR:
        break;

      case VK_ERROR_OUT_OF_DATE_KHR:
        RecreateSwapChain();
        return;
        break;

      default:
        VkThrow(vkAcquireNextImageKHR, acquire_result);
        break;
    }
  }

  // Check if a previous frame is using this image (i.e. there is its fence to
  // wait on)
  if (auto fence = images_in_flight_[image_index]; fence != VK_NULL_HANDLE) {
    VkWrap(vkWaitForFences)(device_, 1u, &fence, kVkTrue, UINT64_MAX);
  }

  // Mark the image as now being in use by this frame
  images_in_flight_[image_index] = in_flight_fences_[current_frame_];

  UpdateUniformBuffer(image_index);

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  const std::array wait_semaphores{image_available_semaphores_[current_frame_]};
  const ui32 num_wait_semaphores = static_cast<ui32>(wait_semaphores.size());
  const std::array signal_semaphores{
      render_finished_semaphores_[current_frame_]};
  const ui32 num_signal_semaphores =
      static_cast<ui32>(signal_semaphores.size());
  const std::array swap_chains{swap_chain_};
  const ui32 num_swap_chains = static_cast<ui32>(swap_chains.size());

  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submit_info.waitSemaphoreCount = num_wait_semaphores;
  submit_info.pWaitSemaphores = wait_semaphores.data();
  submit_info.pWaitDstStageMask = waitStages;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffers_[image_index];
  submit_info.signalSemaphoreCount = num_signal_semaphores;
  submit_info.pSignalSemaphores = signal_semaphores.data();

  VkWrap(vkResetFences)(device_, 1u, &in_flight_fences_[current_frame_]);
  VkWrap(vkQueueSubmit)(graphics_queue_, 1u, &submit_info,
                        in_flight_fences_[current_frame_]);

  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = num_wait_semaphores;
  present_info.pWaitSemaphores = signal_semaphores.data();
  present_info.swapchainCount = num_swap_chains;
  present_info.pSwapchains = swap_chains.data();
  present_info.pImageIndices = &image_index;
  present_info.pResults = nullptr;

  {
    const VkResult present_result =
        vkQueuePresentKHR(present_queue_, &present_info);
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR ||
        present_result == VK_SUBOPTIMAL_KHR || frame_buffer_resized_) {
      frame_buffer_resized_ = false;
      RecreateSwapChain();
    } else if (present_result != VK_SUCCESS) {
      VkThrow(vkQueuePresentKHR, present_result);
    }
  }

  vkDeviceWaitIdle(device_);

  current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
}

void Application::UpdateUniformBuffer(ui32 current_image) {
  UniformBufferObject ubo{};
  const float time = std::chrono::duration<float, std::chrono::seconds::period>(
                         GetTimeSinceAppStart())
                         .count();
  ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                          glm::vec3(0.0f, 0.0f, 1.0f));
  const float distance = 1.0f + std::abs(std::sin(time));
  ubo.view =
      glm::lookAt(glm::vec3(distance, distance, distance),
                  glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.proj = glm::perspective(glm::radians(45.0f),
                              static_cast<float>(swap_chain_extent_.width) /
                                  static_cast<float>(swap_chain_extent_.height),
                              0.1f, 10.0f);
  ubo.proj[1][1] *= -1;

  VulkanUtility::MapCopyUnmap(ubo, device_,
                              uniform_buffers_memory_[current_image]);
}

void Application::Cleanup() {
  CleanupSwapChain();

  using Vk = VulkanUtility;

  Vk::Destroy<vkDestroySampler>(device_, texture_sampler_);
  Vk::Destroy<vkDestroyImageView>(device_, texture_image_view_);
  Vk::Destroy<vkDestroyImage>(device_, texture_image_);
  Vk::FreeMemory(device_, texture_image_memory_);

  Vk::Destroy<vkDestroyDescriptorSetLayout>(device_, descriptor_set_layout_);

  Vk::Destroy<vkDestroyBuffer>(device_, vertex_buffer_);
  Vk::FreeMemory(device_, vertex_buffer_memory_);

  Vk::Destroy<vkDestroyBuffer>(device_, index_buffer_);
  Vk::FreeMemory(device_, index_buffer_memory_);

  Vk::Destroy<vkDestroyFence>(device_, in_flight_fences_);
  Vk::Destroy<vkDestroySemaphore>(device_, render_finished_semaphores_);
  Vk::Destroy<vkDestroySemaphore>(device_, image_available_semaphores_);
  Vk::Destroy<vkDestroyCommandPool>(device_, persistent_command_pool_);
  Vk::Destroy<vkDestroyCommandPool>(device_, transient_command_pool_);
  Vk::Destroy<vkDestroyDevice>(device_);

  DestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);

  Vk::Destroy<vkDestroySurfaceKHR>(instance_, surface_);
  Vk::Destroy<vkDestroyInstance>(instance_);

  if (window_) {
    glfwDestroyWindow(window_);
    window_ = nullptr;
  }

  if (glfw_initialized_) {
    glfwTerminate();
    glfw_initialized_ = false;
  }
}

void Application::CleanupSwapChain() {
  using Vk = VulkanUtility;

  Vk::Destroy<vkDestroyImageView>(device_, depth_image_view_);
  Vk::Destroy<vkDestroyImage>(device_, depth_image_);
  Vk::FreeMemory(device_, depth_image_memory_);

  Vk::Destroy<vkDestroyImageView>(device_, color_image_view_);
  Vk::Destroy<vkDestroyImage>(device_, color_image_);
  Vk::FreeMemory(device_, color_image_memory_);

  Vk::Destroy<vkDestroyFramebuffer>(device_, swap_chain_frame_buffers_);
  if (!command_buffers_.empty()) {
    vkFreeCommandBuffers(device_, persistent_command_pool_,
                         static_cast<uint32_t>(command_buffers_.size()),
                         command_buffers_.data());
    command_buffers_.clear();
  }

  Vk::Destroy<vkDestroyPipeline>(device_, graphics_pipeline_);
  Vk::Destroy<vkDestroyPipelineLayout>(device_, pipeline_layout_);
  Vk::Destroy<vkDestroyRenderPass>(device_, render_pass_);
  Vk::Destroy<vkDestroyImageView>(device_, swap_chain_image_views_);
  Vk::Destroy<vkDestroySwapchainKHR>(device_, swap_chain_);
  Vk::Destroy<vkDestroyBuffer>(device_, uniform_buffers_);
  Vk::FreeMemory(device_, uniform_buffers_memory_);
  Vk::Destroy<vkDestroyDescriptorPool>(device_, descriptor_pool_);
}

VkSurfaceFormatKHR Application::ChooseSurfaceFormat() const {
  constexpr VkSurfaceFormatKHR preferred_format{
      VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

  return surface_info_->ChooseSurfaceFormat(preferred_format);
}

VkPresentModeKHR Application::ChoosePresentMode() const {
  constexpr std::array priority{
      VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_RELAXED_KHR,
      VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_MAILBOX_KHR};

  size_t best_index = 0;
  int best_score = -1;
  auto& modes = surface_info_->present_modes;
  for (size_t i = 0; i < modes.size(); ++i) {
    auto it = std::find(priority.begin(), priority.end(), modes[i]);
    if (it != priority.end()) {
      int score = static_cast<int>(std::distance(priority.begin(), it));
      if (score > best_score) {
        best_score = score;
        best_index = i;
      }
    }
  }

  return surface_info_->present_modes[best_index];
}

std::filesystem::path Application::GetContentDir() const noexcept {
  return executable_file_.parent_path() / "content";
}

std::filesystem::path Application::GetShadersDir() const noexcept {
  return GetContentDir() / "shaders";
}

std::filesystem::path Application::GetTexturesDir() const noexcept {
  return GetContentDir() / "textures";
}

std::filesystem::path Application::GetModelsDir() const noexcept {
  return GetContentDir() / "models";
}

VkFormat Application::SelectDepthFormat() const {
  constexpr std::array formats = {VK_FORMAT_D32_SFLOAT,
                                  VK_FORMAT_D32_SFLOAT_S8_UINT,
                                  VK_FORMAT_D24_UNORM_S8_UINT};
  constexpr auto tiling = GetDepthImageTiling();
  constexpr auto format_features =
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
  return device_info_->GetSupportedFormat(formats, tiling, format_features);
}

VkFormat Application::GetDepthFormat() {
  [[unlikely]] if (!depth_format_.has_value()) {
    depth_format_ = SelectDepthFormat();
  }

  return *depth_format_;
}

void Application::CreateGpuBufferRaw(const void* data, VkDeviceSize buffer_size,
                                     VkBufferUsageFlags usage_flags,
                                     VkBuffer& buffer,
                                     VkDeviceMemory& buffer_memory) {
  VkBuffer staging_buffer = nullptr;
  VkDeviceMemory staging_buffer_memory = nullptr;
  CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer, staging_buffer_memory);

  VulkanUtility::MapCopyUnmap(data, buffer_size, device_,
                              staging_buffer_memory);

  // buffer is device local -
  // it receives data by copying it from the staging buffer
  CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage_flags,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, buffer_memory);

  ExecuteSingleTimeCommands([&](VkCommandBuffer command_buffer) {
    CopyBuffer(command_buffer, staging_buffer, buffer, buffer_size);
  });

  VulkanUtility::Destroy<vkDestroyBuffer>(device_, staging_buffer);
  VulkanUtility::FreeMemory(device_, staging_buffer_memory);
}

VkExtent2D Application::ChooseSwapExtent() const {
  auto& capabilities = surface_info_->capabilities;

  if (capabilities.currentExtent.width != UINT32_MAX) {
    return capabilities.currentExtent;
  }

  int width, height;
  glfwGetFramebufferSize(window_, &width, &height);

  VkExtent2D actualExtent{static_cast<uint32_t>(width),
                          static_cast<uint32_t>(height)};

  actualExtent.width =
      std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                 capabilities.maxImageExtent.width);
  actualExtent.height =
      std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                 capabilities.maxImageExtent.height);

  return actualExtent;
}

std::vector<const char*> Application::GetRequiredExtensions() {
  ui32 num_glfw_ext = 0;
  const char** glfw_extensions =
      glfwGetRequiredInstanceExtensions(&num_glfw_ext);
  std::vector<const char*> extensions(glfw_extensions,
                                      glfw_extensions + num_glfw_ext);

  if constexpr (kEnableDebugUtilsExtension) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  return extensions;
}

void Application::CreateSyncObjects() {
  auto make_semaphore = [dev = device_]() {
    VkSemaphore semaphore;
    VkSemaphoreCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkWrap(vkCreateSemaphore)(dev, &create_info, nullptr, &semaphore);
    return semaphore;
  };

  auto make_fence = [dev = device_]() {
    VkFence fence;
    VkFenceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VkWrap(vkCreateFence)(dev, &create_info, nullptr, &fence);
    return fence;
  };

  auto make_n = [](ui32 n, auto& make_one) {
    std::vector<decltype(make_one())> semaphores;
    semaphores.reserve(n);
    for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
      semaphores.push_back(make_one());
    }

    return semaphores;
  };

  image_available_semaphores_ = make_n(kMaxFramesInFlight, make_semaphore);
  render_finished_semaphores_ = make_n(kMaxFramesInFlight, make_semaphore);
  in_flight_fences_ = make_n(kMaxFramesInFlight, make_fence);
  images_in_flight_.resize(swap_chain_image_views_.size(), nullptr);
}

VkImageView Application::CreateImageView(VkImage image, VkFormat format,
                                         VkImageAspectFlags aspect_flags,
                                         ui32 mip_levels) {
  VkImageViewCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  create_info.image = image;
  create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  create_info.format = format;
  create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  create_info.subresourceRange.aspectMask = aspect_flags;
  create_info.subresourceRange.baseMipLevel = 0;
  create_info.subresourceRange.levelCount = mip_levels;
  create_info.subresourceRange.baseArrayLayer = 0;
  create_info.subresourceRange.layerCount = 1;

  VkImageView image_view = nullptr;
  VkWrap(vkCreateImageView)(device_, &create_info, nullptr, &image_view);
  return image_view;
}
