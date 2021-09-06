#include "application.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string_view>
#include <cstring>
#include <chrono>

#include <GLFW/glfw3.h>
#include "spdlog/spdlog.h"
#include "fmt/format.h"

#include "error_handling.h"
#include "unused_var.h"
#include "vulkan_utility.h"
#include "read_file.h"

#include "pipeline/descriptors/vertex_descriptor.h"
#include "pipeline/uniform_buffer_object.h"

#include "glm/gtc/matrix_transform.hpp"

static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func && debugMessenger) {
        func(instance, debugMessenger, pAllocator);
    }
}

static const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, { 1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f}, { 0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f}, { 0.0f, 0.0f, 1.0f}},
    {{-0.5f,  0.5f}, { 1.0f, 1.0f, 1.0f}}
};

const std::vector<ui16> indices = {
    0, 1, 2, 2, 3, 0
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    unused_var(pUserData, messageType);

    switch(severity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            spdlog::trace("validation layer: {}\n", pCallbackData->pMessage);
            break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            spdlog::info("validation layer: {}\n", pCallbackData->pMessage);
            break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            spdlog::warn("validation layer: {}\n", pCallbackData->pMessage);
            break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            spdlog::error("validation layer: {}\n", pCallbackData->pMessage);
            break;

        default:
            assert(false);
            break;
    }

    return VK_FALSE;
}

Application::Application()
{
    spdlog::set_level(spdlog::level::trace);

    glfw_initialized_ = false;
    frame_buffer_resized_ = false;

#ifndef NDEBUG
    required_layers_.push_back("VK_LAYER_KHRONOS_validation");
#endif
    required_layers_.push_back("VK_LAYER_MESA_overlay");

    device_extensions_.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    app_start_time_ = get_global_time();

}

Application::~Application()
{
    cleanup();
}

void Application::set_executable_file(std::filesystem::path path)
{
    executable_file_ = path;
}

void Application::run()
{
    initialize_window();
    initialize_vulkan();
    main_loop();
    cleanup();
}

void Application::initialize_window()
{
    glfwInit();
    glfw_initialized_ = true;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window_ = glfwCreateWindow(window_width_, window_height_, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, Application::frame_buffer_resize_callback);
}

void Application::frame_buffer_resize_callback(GLFWwindow* window, int width, int height)
{
    unused_var(width, height);
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    app->frame_buffer_resized_ = true;
}

void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& create_info)
{
    create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debug_callback;
    create_info.pUserData = nullptr;
}

void Application::pick_physical_device()
{
    std::vector<VkPhysicalDevice> devices;
    VulkanUtility::get_devices(instance_, devices);

    [[unlikely]]
    if (devices.empty())
    {
        throw std::runtime_error("There is no vulkan capable devices");
    }

    device_info_ = std::make_unique<PhysicalDeviceInfo>();
    surface_info_ = std::make_unique<DeviceSurfaceInfo>();

    int best_score = -1;
    for(size_t i = 0; i < devices.size(); ++i)
    {
        PhysicalDeviceInfo device_info;
        device_info.populate(devices[i], surface_);

        bool has_extensions = true;
        for (auto& required_extension : device_extensions_)
        {
            if (!device_info.has_extension(required_extension))
            {
                has_extensions = false;
                break;
            }
        }

        if (!has_extensions)
        {
            continue;
        }

        // Check swapchain compatibility
        DeviceSurfaceInfo surface_info;
        surface_info.populate(devices[i], surface_);
        if (surface_info.formats.empty() ||
            surface_info.present_modes.empty())
        {
            continue;
        }

        const int score = device_info.rate_device();
        if (score > best_score)
        {
            best_score = score;
            *device_info_ = std::move(device_info);
            *surface_info_ = std::move(surface_info);
        }
    }

    [[unlikely]]
    if (best_score < 0)
    {
        throw std::runtime_error("There is no suitable device");
    }

    spdlog::info("picked device: {}\n", device_info_->properties.deviceName);
}

void Application::create_surface()
{
    vk_wrap(glfwCreateWindowSurface)(instance_, window_, nullptr, &surface_);
}


void Application::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& buffer_memory) const
{
    VkBufferCreateInfo buffer_info {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vk_wrap(vkCreateBuffer)(device_, &buffer_info, nullptr, &buffer);

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(device_, buffer, &memory_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = device_info_->get_memory_type_index(memory_requirements.memoryTypeBits, properties);
    
    vk_wrap(vkAllocateMemory)(device_, &alloc_info, nullptr, &buffer_memory);
    vk_wrap(vkBindBufferMemory)(device_, buffer, buffer_memory, 0);
}

void Application::create_device()
{
    float queue_priority = 1.0f;

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

    auto add_queue_family = [&](ui32 idx)
    {
        if (std::find_if(queue_create_infos.begin(), queue_create_infos.end(),
            [idx](auto& info){ return info.queueFamilyIndex == idx; }) == queue_create_infos.end())
        {
            VkDeviceQueueCreateInfo queue_create_info{};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = idx;
            queue_create_info.queueCount = 1;
            queue_create_info.pQueuePriorities = &queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }
    };

    add_queue_family(device_info_->get_graphics_queue_family_index());
    add_queue_family(device_info_->get_present_queue_family_index());

    VkPhysicalDeviceFeatures device_features{};

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.queueCreateInfoCount = static_cast<ui32>(queue_create_infos.size());
    device_create_info.pEnabledFeatures = &device_features;
    device_create_info.enabledLayerCount = 0; // need to specify them in instance only
    device_create_info.enabledExtensionCount = static_cast<ui32>(device_extensions_.size());
    if (!device_extensions_.empty())
    {
        device_create_info.ppEnabledExtensionNames = device_extensions_.data();
    }

    VkDevice logical_device;
    vk_wrap(vkCreateDevice)(device_info_->device, &device_create_info, nullptr, &logical_device);

    device_ = logical_device;
    vkGetDeviceQueue(device_, device_info_->get_graphics_queue_family_index(), 0, &graphics_queue_);
    vkGetDeviceQueue(device_, device_info_->get_present_queue_family_index(), 0, &present_queue_);
}

void Application::create_swap_chain()
{
    surface_info_->populate(device_info_->device, surface_);
    const VkSurfaceFormatKHR surfaceFormat = choose_surface_format();
    const VkPresentModeKHR presentMode = choose_present_mode();
    swap_chain_extent_ = choose_swap_extent();
    
    ui32 image_count = surface_info_->capabilities.minImageCount + 1;
    if (surface_info_->capabilities.maxImageCount > 0 &&
        image_count > surface_info_->capabilities.maxImageCount)
    {
        image_count = surface_info_->capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface_;
    createInfo.minImageCount = image_count;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = swap_chain_extent_;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    std::array queueFamilyIndices =
    {
        device_info_->get_graphics_queue_family_index(),
        device_info_->get_present_queue_family_index()
    };

    if (queueFamilyIndices[0] != queueFamilyIndices[1])
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = static_cast<ui32>(queueFamilyIndices.size());
        createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = surface_info_->capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    vk_wrap(vkCreateSwapchainKHR)(device_, &createInfo, nullptr, &swap_chain_);

    VulkanUtility::get_swap_chain_images(device_, swap_chain_, swap_chain_images_);
    swap_chain_image_format_ = surfaceFormat.format;
}

void Application::create_swap_chain_image_views()
{
    const size_t num_images = swap_chain_images_.size();
    swap_chain_image_views_.resize(num_images);
    for (size_t i = 0; i != num_images; i++)
    {
        const VkImage image = swap_chain_images_[i];
        VkImageView& image_view = swap_chain_image_views_[i];

        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swap_chain_image_format_;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        vk_wrap(vkCreateImageView)(device_, &createInfo, nullptr, &image_view);
    }
}

void Application::create_render_pass()
{
    VkAttachmentDescription color_attachment{};
    color_attachment.format = swap_chain_image_format_;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_create_info{};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = 1;
    render_pass_create_info.pAttachments = &color_attachment;
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;
    render_pass_create_info.dependencyCount = 1;
    render_pass_create_info.pDependencies = &dependency;

    vk_wrap(vkCreateRenderPass)(device_, &render_pass_create_info, nullptr, &render_pass_);
}

void Application::create_descriptor_set_layout()
{
    VkDescriptorSetLayoutBinding ubo_layout_binding{};
    ubo_layout_binding.binding = 0;
    ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_layout_binding.descriptorCount = 1;
    ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    ubo_layout_binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo create_info {};
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    create_info.bindingCount = 1;
    create_info.pBindings = &ubo_layout_binding;

    vk_wrap(vkCreateDescriptorSetLayout)(device_, &create_info, nullptr, &descriptor_set_layout_);
}

void Application::create_graphics_pipeline()
{
    const auto shaders_dir = get_shaders_dir();

    std::vector<char> cache;
    VkShaderModule vert_shader_module = create_shader_module(shaders_dir / "vertex_shader.spv", cache);
    VkShaderModule fragment_shader_module = create_shader_module(shaders_dir / "fragment_shader.spv", cache);

    VkPipelineShaderStageCreateInfo vert_shader_stage_create_info{};
    vert_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_create_info.module = vert_shader_module;
    vert_shader_stage_create_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_shader_stage_create_info{};
    frag_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_create_info.module = fragment_shader_module;
    frag_shader_stage_create_info.pName = "main";

    auto binding_descriptions = StructDescriptor<Vertex>::get_binding_description();
    auto attribute_descriptions = StructDescriptor<Vertex>::get_input_attribute_descriptions();

    VkPipelineVertexInputStateCreateInfo vert_input_info{};
    vert_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vert_input_info.vertexBindingDescriptionCount = static_cast<ui32>(binding_descriptions.size());
    vert_input_info.pVertexBindingDescriptions = binding_descriptions.data();
    vert_input_info.vertexAttributeDescriptionCount = static_cast<ui32>(attribute_descriptions.size());
    vert_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swap_chain_extent_.width);
    viewport.height = static_cast<float>(swap_chain_extent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swap_chain_extent_;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY; // Optional
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &colorBlendAttachment;
    color_blending.blendConstants[0] = 0.0f; // Optional
    color_blending.blendConstants[1] = 0.0f; // Optional
    color_blending.blendConstants[2] = 0.0f; // Optional
    color_blending.blendConstants[3] = 0.0f; // Optional

    VkPipelineLayoutCreateInfo pipline_layout_info{};
    pipline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipline_layout_info.setLayoutCount = 1;
    pipline_layout_info.pSetLayouts = &descriptor_set_layout_;
    pipline_layout_info.pushConstantRangeCount = 0; // Optional
    pipline_layout_info.pPushConstantRanges = nullptr; // Optional
    vk_wrap(vkCreatePipelineLayout)(device_, &pipline_layout_info, nullptr, &pipeline_layout_);

    std::array shader_stages{ vert_shader_stage_create_info, frag_shader_stage_create_info };
    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = static_cast<ui32>(shader_stages.size());
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vert_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = nullptr; // Optional
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = nullptr; // Optional
    pipeline_info.layout = pipeline_layout_;
    pipeline_info.renderPass = render_pass_;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipeline_info.basePipelineIndex = -1; // Optional

    vk_wrap(vkCreateGraphicsPipelines)(device_, nullptr, 1, &pipeline_info, nullptr, &graphics_pipeline_);

    VulkanUtility::destroy<vkDestroyShaderModule>(device_, vert_shader_module);
    VulkanUtility::destroy<vkDestroyShaderModule>(device_, fragment_shader_module);
}

void Application::create_frame_buffers()
{
    const size_t num_images = static_cast<ui32>(swap_chain_image_views_.size());
    swap_chain_frame_buffers_.resize(num_images);

    for (size_t index = 0; index < num_images; ++index)
    {
        std::array attachments{ swap_chain_image_views_[index] };

        VkFramebufferCreateInfo frame_buffer_info{};
        frame_buffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frame_buffer_info.renderPass = render_pass_;
        frame_buffer_info.attachmentCount = static_cast<ui32>(attachments.size());
        frame_buffer_info.pAttachments = attachments.data();
        frame_buffer_info.width = swap_chain_extent_.width;
        frame_buffer_info.height = swap_chain_extent_.height;
        frame_buffer_info.layers = 1;

        vk_wrap(vkCreateFramebuffer)(device_, &frame_buffer_info, nullptr, &swap_chain_frame_buffers_[index]);
    }
}

VkCommandPool Application::create_command_pool(ui32 queue_family_index, VkCommandPoolCreateFlags flags) const
{
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family_index;
    pool_info.flags = flags;

    VkCommandPool pool;
    vk_wrap(vkCreateCommandPool)(device_, &pool_info, nullptr, &pool);
    return pool;
}

void Application::create_command_pools()
{
    persistent_command_pool_ = create_command_pool(device_info_->get_graphics_queue_family_index());
    transient_command_pool_ = create_command_pool(device_info_->get_graphics_queue_family_index(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
}

void Application::create_vertex_buffers()
{
    create_gpu_buffer(std::span(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertex_buffer_, vertex_buffer_memory_);
}

void Application::create_index_buffers()
{
    create_gpu_buffer(std::span(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, index_buffer_, index_buffer_memory_);
}

void Application::create_uniform_buffers()
{
    VkDeviceSize buffer_size = sizeof(UniformBufferObject);

    const VkBufferUsageFlags buffer_usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    const VkMemoryPropertyFlags buffer_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    const size_t num_buffers = swap_chain_images_.size();
    uniform_buffers_.resize(num_buffers);
    uniform_buffers_memory_.resize(num_buffers);
    for(size_t i = 0; i < num_buffers; ++i)
    {
        create_buffer(buffer_size, buffer_usage, buffer_flags, uniform_buffers_[i], uniform_buffers_memory_[i]);
    }
}

void Application::create_descriptor_pool()
{
    VkDescriptorPoolSize pool_size {};
    pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = static_cast<ui32>(swap_chain_images_.size());

    std::array pool_sizes = { pool_size };

    VkDescriptorPoolCreateInfo pool_info {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = static_cast<ui32>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = static_cast<ui32>(swap_chain_images_.size());

    vk_wrap(vkCreateDescriptorPool)(device_, &pool_info, nullptr, &descriptor_pool_);
}

void Application::create_descriptor_sets()
{
    const ui32 num_descriptor_sets = static_cast<ui32>(swap_chain_images_.size());
    std::vector<VkDescriptorSetLayout> layouts(num_descriptor_sets, descriptor_set_layout_);

    VkDescriptorSetAllocateInfo alloc_info {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool_;
    alloc_info.descriptorSetCount = num_descriptor_sets;
    alloc_info.pSetLayouts = layouts.data();

    descriptor_sets_.resize(num_descriptor_sets);
    vk_wrap(vkAllocateDescriptorSets)(device_, &alloc_info, descriptor_sets_.data());

    for(ui32 i = 0; i < num_descriptor_sets; ++i)
    {
        VkDescriptorBufferInfo buffer_info {};
        buffer_info.buffer = uniform_buffers_[i];
        buffer_info.offset = 0;
        buffer_info.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptor_set_write{};
        descriptor_set_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_set_write.dstSet = descriptor_sets_[i];
        descriptor_set_write.dstBinding = 0;
        descriptor_set_write.dstArrayElement = 0;
        descriptor_set_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_set_write.descriptorCount = 1;
        descriptor_set_write.pBufferInfo = &buffer_info;
        descriptor_set_write.pImageInfo = nullptr; // Optional
        descriptor_set_write.pTexelBufferView = nullptr; // Optional

        vkUpdateDescriptorSets(device_, 1, &descriptor_set_write, 0, nullptr);
    }
}

void Application::copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = transient_command_pool_;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer = nullptr;
    vk_wrap(vkAllocateCommandBuffers)(device_, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk_wrap(vkBeginCommandBuffer)(command_buffer, &begin_info);

    VkBufferCopy copy_region{};
    copy_region.size = size;
    vkCmdCopyBuffer(command_buffer, src, dst, 1, &copy_region);

    vk_wrap(vkEndCommandBuffer)(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    vk_wrap(vkQueueSubmit)(graphics_queue_, 1, &submit_info, nullptr);
    vk_wrap(vkQueueWaitIdle)(graphics_queue_);
    vkFreeCommandBuffers(device_, transient_command_pool_, 1, &command_buffer);
}

void Application::create_command_buffers()
{
    ui32 num_buffers = static_cast<ui32>(swap_chain_frame_buffers_.size());
    command_buffers_.resize(num_buffers);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = persistent_command_pool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = num_buffers;
    vk_wrap(vkAllocateCommandBuffers)(device_, &allocInfo, command_buffers_.data());

    for (ui32 i = 0; i != num_buffers; ++i)
    {
        VkCommandBuffer command_buffer = command_buffers_[i];

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        vk_wrap(vkBeginCommandBuffer)(command_buffer, &beginInfo);

        VkRenderPassBeginInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = render_pass_;
        render_pass_info.framebuffer = swap_chain_frame_buffers_[i];
        render_pass_info.renderArea.offset = { 0, 0 };
        render_pass_info.renderArea.extent = swap_chain_extent_;

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        render_pass_info.clearValueCount = 1;
        render_pass_info.pClearValues = &clearColor;

        vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        auto draw_frame_label = annotate_.ScopedLabel(command_buffer, "draw frame", LabelColor::Green());
        {
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);

            std::array vertex_buffers { vertex_buffer_ };
            const ui32 num_vertex_buffers = static_cast<ui32>(vertex_buffers.size());
            std::array offsets { VkDeviceSize(0) };
            vkCmdBindVertexBuffers(command_buffer, 0, num_vertex_buffers, vertex_buffers.data(), offsets.data());
            vkCmdBindIndexBuffer(command_buffer, index_buffer_, 0, VK_INDEX_TYPE_UINT16);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                &descriptor_sets_[i], 0, 0);
            
            ui32 num_instances = 1;
            const ui32 num_indices = static_cast<ui32>(indices.size());
            vkCmdDrawIndexed(command_buffer, num_indices, num_instances, 0, 0, 0);
        }
        
        vkCmdEndRenderPass(command_buffer);

        vk_wrap(vkEndCommandBuffer)(command_buffer);
    }
}

VkShaderModule Application::create_shader_module(const std::filesystem::path& file, std::vector<char>& shader_code)
{
    read_file(file, shader_code);

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = shader_code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(shader_code.data());
    VkShaderModule shader_module;
    vk_wrap(vkCreateShaderModule)(device_, &create_info, nullptr, &shader_module);

    return shader_module;
}

void Application::check_required_layers_support()
{
    if (required_layers_.empty())
    {
        return;
    }

    ui32 layer_count;
    vk_wrap(vkEnumerateInstanceLayerProperties)(&layer_count, nullptr);

    std::vector<VkLayerProperties> availableLayers(layer_count);
    vk_wrap(vkEnumerateInstanceLayerProperties)(&layer_count, availableLayers.data());

    for (const auto& layer_name : required_layers_)
    {
        std::string_view name_view(layer_name);
        const auto it = std::find_if(availableLayers.begin(), availableLayers.end(), [&](const VkLayerProperties& layer_properties)
        {
            return name_view == layer_properties.layerName;
        });

        [[unlikely]]
        if (it == availableLayers.end())
        {
            auto message = fmt::format("{} layer is not present", name_view);
            throw std::runtime_error(std::move(message));
        }
    }
}

void Application::initialize_vulkan()
{
    create_instance();
    annotate_.Initialize(instance_);
    setup_debug_messenger();
    create_surface();
    pick_physical_device();
    create_device();
    create_swap_chain();
    create_swap_chain_image_views();
    create_render_pass();
    create_descriptor_set_layout();
    create_graphics_pipeline();
    create_frame_buffers();
    create_command_pools();
    create_vertex_buffers();
    create_index_buffers();
    create_uniform_buffers();
    create_descriptor_pool();
    create_descriptor_sets();
    create_command_buffers();
    create_sync_objects();
}

void Application::recreate_swap_chain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    if (device_)
    {
        vk_wrap(vkDeviceWaitIdle)(device_);
    }

    cleanup_swap_chain();
    create_swap_chain();
    create_swap_chain_image_views();
    create_render_pass();
    create_graphics_pipeline();
    create_frame_buffers();
    create_uniform_buffers();
    create_descriptor_pool();
    create_descriptor_sets();
    create_command_buffers();
}

void Application::create_instance()
{
    check_required_layers_support();

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Hello Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    auto required_extensions = get_required_extensions();

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<ui32>(required_extensions.size());
    create_info.ppEnabledExtensionNames = required_extensions.data();
    create_info.enabledLayerCount = static_cast<ui32>(required_layers_.size());

    VkDebugUtilsMessengerCreateInfoEXT create_messenger_info{};
    if (create_info.enabledLayerCount > 0)
    {
        create_info.ppEnabledLayerNames = required_layers_.data();
        populate_debug_messenger_create_info(create_messenger_info);
        create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&create_messenger_info;
    }

    vk_wrap(vkCreateInstance)(&create_info, nullptr, &instance_);
}

void Application::setup_debug_messenger()
{
    VkDebugUtilsMessengerCreateInfoEXT create_info {};
    populate_debug_messenger_create_info(create_info);
    vk_wrap(CreateDebugUtilsMessengerEXT)(instance_, &create_info, nullptr, &debug_messenger_);
}

void Application::main_loop()
{
    while (!glfwWindowShouldClose(window_))
    {
        glfwPollEvents();
        draw_frame();
    }
}

void Application::draw_frame()
{
    // first check that nobody does not draw to current frame
    vk_wrap(vkWaitForFences)(device_, 1, &in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);

    // get next image index from the swap chain
    ui32 image_index;

    {
        const VkResult acquire_result = vkAcquireNextImageKHR(device_, swap_chain_,
            UINT64_MAX, image_available_semaphores_[current_frame_], VK_NULL_HANDLE, &image_index);

        switch (acquire_result)
        {
        case VK_SUCCESS:
        case VK_SUBOPTIMAL_KHR:
            break;

        case VK_ERROR_OUT_OF_DATE_KHR:
            recreate_swap_chain();
            return;
            break;

        default:
            vk_throw(vkAcquireNextImageKHR, acquire_result);
            break;
        }
    }

    // Check if a previous frame is using this image (i.e. there is its fence to wait on)
    if (auto fence = images_in_flight_[image_index]; fence != VK_NULL_HANDLE)
    {
        vk_wrap(vkWaitForFences)(device_, 1, &fence, VK_TRUE, UINT64_MAX);
    }

    // Mark the image as now being in use by this frame
    images_in_flight_[image_index] = in_flight_fences_[current_frame_];

    update_uniform_buffer(image_index);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    const std::array wait_semaphores{ image_available_semaphores_[current_frame_] };
    const ui32 num_wait_semaphores = static_cast<ui32>(wait_semaphores.size());
    const std::array signal_semaphores{ render_finished_semaphores_[current_frame_] };
    const ui32 num_signal_semaphores = static_cast<ui32>(signal_semaphores.size());
    const std::array swap_chains{ swap_chain_ };
    const ui32 num_swap_chains = static_cast<ui32>(swap_chains.size());

    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = num_wait_semaphores;
    submit_info.pWaitSemaphores = wait_semaphores.data();
    submit_info.pWaitDstStageMask = waitStages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffers_[image_index];
    submit_info.signalSemaphoreCount = num_signal_semaphores;
    submit_info.pSignalSemaphores = signal_semaphores.data();

    vk_wrap(vkResetFences)(device_, 1, &in_flight_fences_[current_frame_]);
    vk_wrap(vkQueueSubmit)(graphics_queue_, 1, &submit_info, in_flight_fences_[current_frame_]);

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = num_wait_semaphores;
    present_info.pWaitSemaphores = signal_semaphores.data();
    present_info.swapchainCount = num_swap_chains;
    present_info.pSwapchains = swap_chains.data();
    present_info.pImageIndices = &image_index;
    present_info.pResults = nullptr;

    {
        const VkResult present_result = vkQueuePresentKHR(present_queue_, &present_info);
        if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR || frame_buffer_resized_)
        {
            frame_buffer_resized_ = false;
            recreate_swap_chain();
        }
        else if (present_result != VK_SUCCESS)
        {
            vk_throw(vkQueuePresentKHR, present_result);
        }
    }

    vkDeviceWaitIdle(device_);

    current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
}

static void MapCopyUnmap(const void* data, VkDeviceSize size,
    VkDevice device, VkDeviceMemory device_memory,
    VkDeviceSize offset = 0, VkMemoryMapFlags mem_map_flags = 0)
{
    void* mapped = nullptr;
    vk_wrap(vkMapMemory)(device, device_memory, offset, size, mem_map_flags, &mapped);
    std::memcpy(mapped, data, size);
    vkUnmapMemory(device, device_memory);
}

template<typename T>
void MapCopyUnmap(T& object, VkDevice device, VkDeviceMemory device_memory,
    VkDeviceSize offset = 0, VkMemoryMapFlags mem_map_flags = 0)
{
    MapCopyUnmap(&object, sizeof(T), device, device_memory, offset, mem_map_flags);
}

void Application::update_uniform_buffer(ui32 current_image)
{
    UniformBufferObject ubo{};
    const float time = std::chrono::duration<float, std::chrono::seconds::period>(get_time_since_app_start()).count();
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), swap_chain_extent_.width / (float)swap_chain_extent_.height, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;

    MapCopyUnmap(ubo, device_, uniform_buffers_memory_[current_image]);
}

void Application::cleanup()
{
    cleanup_swap_chain();

    using Vk = VulkanUtility;

    Vk::destroy<vkDestroyDescriptorSetLayout>(device_, descriptor_set_layout_);

    Vk::destroy<vkDestroyBuffer>(device_, vertex_buffer_);
    Vk::free_memory(device_, vertex_buffer_memory_);

    Vk::destroy<vkDestroyBuffer>(device_, index_buffer_);
    Vk::free_memory(device_, index_buffer_memory_);

    Vk::destroy<vkDestroyFence>(device_, in_flight_fences_);
    Vk::destroy<vkDestroySemaphore>(device_, render_finished_semaphores_);
    Vk::destroy<vkDestroySemaphore>(device_, image_available_semaphores_);
    Vk::destroy<vkDestroyCommandPool>(device_, persistent_command_pool_);
    Vk::destroy<vkDestroyCommandPool>(device_, transient_command_pool_);
    Vk::destroy<vkDestroyDevice>(device_);

    DestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);

    Vk::destroy<vkDestroySurfaceKHR>(instance_, surface_);
    Vk::destroy<vkDestroyInstance>(instance_);

    if (window_)
    {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    
    if (glfw_initialized_)
    {
        glfwTerminate();
        glfw_initialized_ = false;
    }
}

void Application::cleanup_swap_chain()
{
    using Vk = VulkanUtility;

    Vk::destroy<vkDestroyFramebuffer>(device_, swap_chain_frame_buffers_);
    if (!command_buffers_.empty())
    {
        vkFreeCommandBuffers(device_, persistent_command_pool_, static_cast<uint32_t>(command_buffers_.size()), command_buffers_.data());
        command_buffers_.clear();
    }

    Vk::destroy<vkDestroyPipeline>(device_, graphics_pipeline_);
    Vk::destroy<vkDestroyPipelineLayout>(device_, pipeline_layout_);
    Vk::destroy<vkDestroyRenderPass>(device_, render_pass_);
    Vk::destroy<vkDestroyImageView>(device_, swap_chain_image_views_);
    Vk::destroy<vkDestroySwapchainKHR>(device_, swap_chain_);
    Vk::destroy<vkDestroyBuffer>(device_, uniform_buffers_);
    Vk::free_memory(device_, uniform_buffers_memory_);
    Vk::destroy<vkDestroyDescriptorPool>(device_, descriptor_pool_);
}

VkSurfaceFormatKHR Application::choose_surface_format() const
{
    constexpr VkSurfaceFormatKHR preferred_format {
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
    };

    return surface_info_->choose_surface_format(preferred_format);
}

VkPresentModeKHR Application::choose_present_mode() const
{
    constexpr std::array priority
    {
        VK_PRESENT_MODE_IMMEDIATE_KHR,
        VK_PRESENT_MODE_FIFO_RELAXED_KHR,
        VK_PRESENT_MODE_FIFO_KHR,
        VK_PRESENT_MODE_MAILBOX_KHR
    };

    size_t best_index = 0;
    int best_score = -1;
    auto& modes = surface_info_->present_modes;
    for (size_t i = 0; i < modes.size(); ++i)
    {
        auto it = std::find(priority.begin(), priority.end(), modes[i]);
        if (it != priority.end())
        {
            int score = static_cast<int>(std::distance(priority.begin(), it));
            if (score > best_score)
            {
                best_score = score;
                best_index = i;
            }
        }
    }

    return surface_info_->present_modes[best_index];
}

std::filesystem::path Application::get_shaders_dir() const noexcept
{
    return executable_file_.parent_path() / "shaders";
}


void Application::create_gpu_buffer_raw(const void* data, VkDeviceSize buffer_size,
    VkBufferUsageFlags usage_flags, VkBuffer& buffer, VkDeviceMemory& buffer_memory)
{
    VkBuffer staging_buffer = nullptr;
    VkDeviceMemory staging_buffer_memory = nullptr;
    create_buffer(buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging_buffer, staging_buffer_memory);

    MapCopyUnmap(data, buffer_size, device_, staging_buffer_memory);

    // buffer is device local -
    // it receives data by copying it from the staging buffer
    create_buffer(buffer_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage_flags,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        buffer, buffer_memory);

    copy_buffer(staging_buffer, buffer, buffer_size);

    VulkanUtility::destroy<vkDestroyBuffer>(device_, staging_buffer);
    VulkanUtility::free_memory(device_, staging_buffer_memory);
}

VkExtent2D Application::choose_swap_extent() const
{
    auto& capabilities = surface_info_->capabilities;

    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(window_, &width, &height);

    VkExtent2D actualExtent {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
}

std::vector<const char*> Application::get_required_extensions()
{
    ui32 num_glfw_ext = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&num_glfw_ext);
    std::vector<const char*> extensions(glfw_extensions, glfw_extensions + num_glfw_ext);

#ifndef NDEBUG
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    return extensions;
}

void Application::create_sync_objects()
{
    auto make_semaphore = [dev = device_]()
    {
        VkSemaphore semaphore;
        VkSemaphoreCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        vk_wrap(vkCreateSemaphore)(dev, &create_info, nullptr, &semaphore);
        return semaphore;
    };

    auto make_fence = [dev = device_]()
    {
        VkFence fence;
        VkFenceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vk_wrap(vkCreateFence)(dev, &create_info, nullptr, &fence);
        return fence;
    };

    auto make_n = [](ui32 n, auto& make_one)
    {
        std::vector<decltype(make_one())> semaphores;
        semaphores.reserve(n);
        for (size_t i = 0; i < kMaxFramesInFlight; ++i)
        {
            semaphores.push_back(make_one());
        }

        return semaphores;
    };

    image_available_semaphores_ = make_n(kMaxFramesInFlight, make_semaphore);
    render_finished_semaphores_ = make_n(kMaxFramesInFlight, make_semaphore);
    in_flight_fences_ = make_n(kMaxFramesInFlight, make_fence);
    images_in_flight_.resize(swap_chain_image_views_.size(), VK_NULL_HANDLE);
}
