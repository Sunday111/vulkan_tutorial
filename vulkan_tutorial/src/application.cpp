#include "application.h"
#include "error_handling.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <string_view>
#include <stdexcept>
#include <cassert>
#include <ranges>
#include <cmath>
#include <fstream>

#include "unused_var.h"
#include "vulkan_utility.h"
#include "device/physical_device_info.h"

template<typename T, typename = std::enable_if_t<sizeof(T) == sizeof(char)>>
void read_file(const std::filesystem::path& path, std::vector<T>& buffer)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    
    [[unlikely]]
    if (!file.is_open())
    {
        auto message = fmt::format("failed to open file {}", path.string());
        throw std::runtime_error(std::move(message));
    }
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    buffer.resize(size);
    
    [[unlikely]]
    if (!file.read((char*)buffer.data(), size))
    {
        auto message = fmt::format("failed to read {} bytes from file {}", size, path.string());
        throw std::runtime_error(std::move(message));
    }
}

void destroy_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func)
    {
        func(instance, debugMessenger, pAllocator);
    }
}

std::string_view serveriity_to_string(VkDebugUtilsMessageSeverityFlagBitsEXT severity)
{
    switch (severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: return "verbose";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: return "info";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: return "warning";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: return "error";
    }

    assert(false);
    return "unknown severity";
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    unused_var(pUserData, messageType);
    fmt::print(
        "validation layer [{}]: {}\n",
        serveriity_to_string(severity), pCallbackData->pMessage);

    return VK_FALSE;
}

Application::Application()
{
    glfw_initialized = false;

#ifndef NDEBUG
    validation_layers_.push_back("VK_LAYER_KHRONOS_validation");
#endif

    device_extensions_.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
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
    glfw_initialized = true;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window_ = glfwCreateWindow(window_width_, window_height_, "Vulkan", nullptr, nullptr);
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

    int best_score = -1;
    for(size_t i = 0; i < devices.size(); ++i)
    {
        PhysicalDeviceInfo device_info;
        device_info.set_device(devices[i], surface_);

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

        // Check swapchain
        if (device_info.swapchain.formats.empty() ||
            device_info.swapchain.present_modes.empty())
        {
            continue;
        }

        const int score = device_info.rate_device();
        if (score > best_score)
        {
            best_score = score;
            device_info_ = std::move(device_info);
        }
    }

    [[unlikely]]
    if (best_score < 0)
    {
        throw std::runtime_error("There is no suitable device");
    }
}

void Application::create_surface()
{
    vk_expect_success(
        glfwCreateWindowSurface(instance_, window_, nullptr, &surface_),
        "glfwCreateWindowSurface");
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

    add_queue_family(device_info_.get_graphics_queue_family_index());
    add_queue_family(device_info_.get_present_queue_family_index());

    VkPhysicalDeviceFeatures device_features{};

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.queueCreateInfoCount = static_cast<ui32>(queue_create_infos.size());
    device_create_info.pEnabledFeatures = &device_features;
    device_create_info.enabledExtensionCount = 0;
    device_create_info.enabledLayerCount = 0; // need to specify them in instance only
    device_create_info.enabledExtensionCount = static_cast<ui32>(device_extensions_.size());
    if (!device_extensions_.empty())
    {
        device_create_info.ppEnabledExtensionNames = device_extensions_.data();
    }

    VkDevice logical_device;
    vk_expect_success(
        vkCreateDevice(device_info_.device, &device_create_info, nullptr, &logical_device),
        "vkCreateDevice - create logical device for {}", device_info_.properties.deviceName);

    device_ = logical_device;
    vkGetDeviceQueue(device_, device_info_.get_graphics_queue_family_index(), 0, &graphics_queue_);
    vkGetDeviceQueue(device_, device_info_.get_present_queue_family_index(), 0, &present_queue_);
}

void Application::create_swap_chain()
{
    const VkSurfaceFormatKHR surfaceFormat = choose_surface_format();
    const VkPresentModeKHR presentMode = choose_present_mode();
    swap_chain_extent_ = choose_swap_extent();
    
    ui32 image_count = device_info_.swapchain.capabilities.minImageCount + 1;
    if (device_info_.swapchain.capabilities.maxImageCount > 0 &&
        image_count > device_info_.swapchain.capabilities.maxImageCount)
    {
        image_count = device_info_.swapchain.capabilities.maxImageCount;
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
        device_info_.get_graphics_queue_family_index(),
        device_info_.get_present_queue_family_index()
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

    createInfo.preTransform = device_info_.swapchain.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    vk_expect_success(
        vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swap_chain_),
        "vkCreateSwapchainKHR");

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

        vk_expect_success(
            vkCreateImageView(device_, &createInfo, nullptr, &image_view),
            "vkCreateImageView");
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

    vk_expect_success(
        vkCreateRenderPass(device_, &render_pass_create_info, nullptr, &render_pass_),
        "vkCreateRenderPass");
}

void Application::create_graphics_pipeline()
{
    auto shaders_dir = executable_file_.parent_path() / "shaders";

    std::vector<ui8> cache;
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

    VkPipelineVertexInputStateCreateInfo vert_input_info{};
    vert_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vert_input_info.vertexBindingDescriptionCount = 0;
    vert_input_info.pVertexBindingDescriptions = nullptr; // Optional
    vert_input_info.vertexAttributeDescriptionCount = 0;
    vert_input_info.pVertexAttributeDescriptions = nullptr;

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
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
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
    pipline_layout_info.setLayoutCount = 0; // Optional
    pipline_layout_info.pSetLayouts = nullptr; // Optional
    pipline_layout_info.pushConstantRangeCount = 0; // Optional
    pipline_layout_info.pPushConstantRanges = nullptr; // Optional

    vk_expect_success(
        vkCreatePipelineLayout(device_, &pipline_layout_info, nullptr, &pipeline_layout_),
        "vkCreatePipelineLayout at {}", __LINE__);

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

    vk_expect_success(
        vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline_),
        "vkCreateGraphicsPipelines at {}", __LINE__);

    vk_destroy<vkDestroyShaderModule>(device_, vert_shader_module);
    vk_destroy<vkDestroyShaderModule>(device_, fragment_shader_module);
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

        vk_expect_success(
            vkCreateFramebuffer(device_, &frame_buffer_info, nullptr, &swap_chain_frame_buffers_[index]),
            "vkCreateFramebuffer [{}/{}] at {}", index, num_images, __LINE__);
    }
}

void Application::create_command_pool()
{
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = device_info_.get_graphics_queue_family_index();
    pool_info.flags = 0; // Optional

    vk_expect_success(
        vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_),
        "vkCreateCommandPool for graphics family at {}", __LINE__);
}

void Application::create_command_buffers()
{
    ui32 num_buffers = static_cast<ui32>(swap_chain_frame_buffers_.size());
    command_buffers_.resize(num_buffers);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = command_pool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = num_buffers;
    vk_expect_success(
        vkAllocateCommandBuffers(device_, &allocInfo, command_buffers_.data()),
        "vkAllocateCommandBuffers {} buffers", num_buffers);

    for (ui32 i = 0; i != num_buffers; ++i)
    {
        VkCommandBuffer command_buffer = command_buffers_[i];

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        vk_expect_success(
            vkBeginCommandBuffer(command_buffer, &beginInfo),
            "vkBeginCommandBuffer {}", i);

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
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);

        ui32 num_vertices = 3;
        ui32 num_instances = 1;
        ui32 first_vertex = 0;
        ui32 first_instance = 0;
        vkCmdDraw(command_buffer, num_vertices, num_instances, first_vertex, first_instance);
        vkCmdEndRenderPass(command_buffer);

        vk_expect_success(
            vkEndCommandBuffer(command_buffer),
            "vkEndCommandBuffer {}", i);
    }
}

VkShaderModule Application::create_shader_module(const std::filesystem::path& file, std::vector<ui8>& shader_code)
{
    read_file(file, shader_code);

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = shader_code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(shader_code.data());
    VkShaderModule shader_module;
    vk_expect_success(
        vkCreateShaderModule(device_, &create_info, nullptr, &shader_module),
        "vkCreateShaderModule");

    return shader_module;
}

void Application::checkValidationLayerSupport()
{
    if (validation_layers_.empty())
    {
        return;
    }

    ui32 layer_count;
    vk_expect_success(vkEnumerateInstanceLayerProperties(&layer_count, nullptr),
        "vkEnumerateInstanceLayerProperties: get validation layers count");

    std::vector<VkLayerProperties> availableLayers(layer_count);
    vk_expect_success(vkEnumerateInstanceLayerProperties(&layer_count, availableLayers.data()),
        "vkEnumerateInstanceLayerProperties: get validation layers info");

    for (const auto& layer_name : validation_layers_)
    {
        std::string_view name_view(layer_name);
        const auto it = std::find_if(availableLayers.begin(), availableLayers.end(), [&](const VkLayerProperties& layer_properties)
        {
            return name_view == layer_properties.layerName;
        });

        [[unlikely]]
        if (it == availableLayers.end())
        {
            auto message = fmt::format("{} validation layer is not present", name_view);
            throw std::runtime_error(std::move(message));
        }
    }
}

void Application::initialize_vulkan()
{
    create_instance();
    create_surface();
    pick_physical_device();
    create_device();
    create_swap_chain();
    create_swap_chain_image_views();
    create_render_pass();
    create_graphics_pipeline();
    create_frame_buffers();
    create_command_pool();
    create_command_buffers();
    create_semaphores();
}

void Application::create_instance()
{
    checkValidationLayerSupport();

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
    create_info.enabledLayerCount = static_cast<ui32>(validation_layers_.size());

    VkDebugUtilsMessengerCreateInfoEXT create_messenger_info{};
    if (create_info.enabledLayerCount > 0)
    {
        create_info.ppEnabledLayerNames = validation_layers_.data();
        populate_debug_messenger_create_info(create_messenger_info);
        create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&create_messenger_info;
    }

    vk_expect_success(vkCreateInstance(&create_info, nullptr, &instance_), "vkCreateInstance");
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
    ui32 image_index;
    vk_expect_success(
        vkAcquireNextImageKHR(device_, swap_chain_, UINT64_MAX, image_available_semaphore_, VK_NULL_HANDLE, &image_index),
        "vkAcquireNextImageKHR");

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    const std::array wait_semaphores{ image_available_semaphore_ };
    const ui32 num_wait_semaphores = static_cast<ui32>(wait_semaphores.size());
    const std::array signal_semaphores{ render_finished_semaphore_ };
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

    vk_expect_success(
        vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE),
        "vkQueueSubmit");

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = num_wait_semaphores;
    present_info.pWaitSemaphores = signal_semaphores.data();
    present_info.swapchainCount = num_swap_chains;
    present_info.pSwapchains = swap_chains.data();
    present_info.pImageIndices = &image_index;
    present_info.pResults = nullptr;

    vk_expect_success(
        vkQueuePresentKHR(present_queue_, &present_info),
        "vkQueuePresentKHR");

    vkDeviceWaitIdle(device_);
}

void Application::cleanup()
{
    vk_destroy<vkDestroySemaphore>(device_, render_finished_semaphore_);
    vk_destroy<vkDestroySemaphore>(device_, image_available_semaphore_);
    vk_destroy<vkDestroyCommandPool>(device_, command_pool_);
    vk_destroy<vkDestroyFramebuffer>(device_, swap_chain_frame_buffers_);
    vk_destroy<vkDestroyPipeline>(device_, graphics_pipeline_);
    vk_destroy<vkDestroyPipelineLayout>(device_, pipeline_layout_);
    vk_destroy<vkDestroyRenderPass>(device_, render_pass_);
    vk_destroy<vkDestroyImageView>(device_, swap_chain_image_views_);
    vk_destroy<vkDestroySwapchainKHR>(device_, swap_chain_);
    vk_destroy<vkDestroyDevice>(device_);
    vk_destroy<vkDestroySurfaceKHR>(instance_, surface_);
    vk_destroy<vkDestroyInstance>(instance_);

    if (window_)
    {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    if (glfw_initialized)
    {
        glfwTerminate();
        glfw_initialized = false;
    }
}

VkSurfaceFormatKHR Application::choose_surface_format() const
{
    constexpr VkSurfaceFormatKHR preferred_format {
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
    };

    return device_info_.swapchain.choose_surface_format(preferred_format);
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
    auto& modes = device_info_.swapchain.present_modes;
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

    return device_info_.swapchain.present_modes[best_index];
}

VkExtent2D Application::choose_swap_extent() const
{
    auto& capabilities = device_info_.swapchain.capabilities;

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

void Application::create_semaphores()
{
    auto make_one = [dev = device_]()
    {
        VkSemaphore semaphore;
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        vk_expect_success(
            vkCreateSemaphore(dev, &semaphoreInfo, nullptr, &semaphore),
            "vkCreateSemaphore for image_available_semaphore_"
        );
        return semaphore;
    };

    image_available_semaphore_ = make_one();
    render_finished_semaphore_ = make_one();
}
