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

#include "unused_var.h"
#include "vulkan_utility.h"
#include "device/physical_device_info.h"
#include "device/vulkan_device.h"
#include <unordered_map>

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
    VulkanUtility::get_devices(vk_instance_, devices);

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
        glfwCreateWindowSurface(vk_instance_, window_, nullptr, &surface_),
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
    VkQueue graphics_queue = device_.get_queue(device_info_.get_graphics_queue_family_index(), 0);
    VkQueue present_queue = device_.get_queue(device_info_.get_present_queue_family_index(), 0);
    unused_var(graphics_queue, present_queue);
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

    vk_expect_success(vkCreateInstance(&create_info, nullptr, &vk_instance_), "vkCreateInstance");
}

void Application::main_loop()
{
    while (!glfwWindowShouldClose(window_))
    {
        glfwPollEvents();
    }
}

void Application::cleanup()
{
    while (!swap_chain_image_views_.empty())
    {
        vkDestroyImageView(device_, swap_chain_image_views_.back(), nullptr);
        swap_chain_image_views_.pop_back();
    }

    if (swap_chain_)
    {
        vkDestroySwapchainKHR(device_, swap_chain_, nullptr);
        swap_chain_ = nullptr;
    }

    device_ = nullptr;

    if (surface_)
    {
        vkDestroySurfaceKHR(vk_instance_, surface_, nullptr);
        surface_ = nullptr;
    }

    if(vk_instance_)
    {
        vkDestroyInstance(vk_instance_, nullptr);
        vk_instance_ = nullptr;
    }

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
