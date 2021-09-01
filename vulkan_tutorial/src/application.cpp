#include "application.h"
#include "error_handling.h"
#include <GLFW/glfw3.h>
#include <array>
#include <string_view>
#include <stdexcept>
#include <cassert>
#include <ranges>

#include "unused_var.h"
#include "vulkan_utility.h"
#include "device/physical_device_info.h"
#include "device/vulkan_device.h"

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

VkPhysicalDevice Application::pick_physical_device() const
{
    std::vector<VkPhysicalDevice> devices;
    VulkanUtility::get_devices(vk_instance_, devices);

    [[unlikely]]
    if (devices.empty())
    {
        throw std::runtime_error("There is no vulkan capable devices");
    }

    int best_score = -1;
    size_t best_device_index = 0;

    PhysicalDeviceInfo device_info;
    for(size_t i = 0; i < devices.size(); ++i)
    {
        device_info.set_device(devices[i], surface_);
        const int score = device_info.rate_device();
        if (score > best_score)
        {
            best_score = score;
            best_device_index = i;
        }
    }

    [[unlikely]]
    if (best_score < 0)
    {
        throw std::runtime_error("There is no suitable device");
    }

    return devices[best_device_index];
}

void Application::create_surface()
{
    vk_expect_success(
        glfwCreateWindowSurface(vk_instance_, window_, nullptr, &surface_),
        "glfwCreateWindowSurface");
}

void Application::create_device()
{
    VkPhysicalDevice phys_dev = pick_physical_device();
    PhysicalDeviceInfo phys_dev_info;
    phys_dev_info.set_device(phys_dev, surface_);

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

    add_queue_family(phys_dev_info.get_graphics_queue_family_index());
    add_queue_family(phys_dev_info.get_present_queue_family_index());

    VkPhysicalDeviceFeatures device_features{};

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.queueCreateInfoCount = static_cast<ui32>(queue_create_infos.size());
    device_create_info.pEnabledFeatures = &device_features;
    device_create_info.enabledExtensionCount = 0;
    device_create_info.enabledLayerCount = 0; // need to specify them in instance only

    VkDevice logical_device;
    vk_expect_success(
        vkCreateDevice(phys_dev, &device_create_info, nullptr, &logical_device),
        "vkCreateDevice - create logical device for {}", phys_dev_info.properties.deviceName);

    device_ = logical_device;
    VkQueue graphics_queue = device_.get_queue(phys_dev_info.get_graphics_queue_family_index(), 0);
    VkQueue present_queue = device_.get_queue(phys_dev_info.get_present_queue_family_index(), 0);
    unused_var(graphics_queue, present_queue);
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
        const auto it = std::find_if(availableLayers.begin(), availableLayers.end(), [&](const VkLayerProperties& layer_properties)
        {
            return layer_name == layer_properties.layerName;
        });

        [[unlikely]]
        if (it == availableLayers.end())
        {
            auto message = fmt::format("{} validation layer is not present", layer_name);
            throw std::runtime_error(std::move(message));
        }
    }
}

void Application::initialize_vulkan()
{
    create_instance();
    create_surface();
    create_device();
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


    std::vector<const char*> layer_names;
    layer_names.reserve(validation_layers_.size());
    for (const std::string& layer_name: validation_layers_)
    {
        layer_names.push_back(layer_name.data());
    }

    VkDebugUtilsMessengerCreateInfoEXT create_messenger_info{};
    if (create_info.enabledLayerCount > 0)
    {
        create_info.ppEnabledLayerNames = layer_names.data();
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
