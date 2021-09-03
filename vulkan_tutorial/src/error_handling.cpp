#include "error_handling.h"

#include <stdexcept>

#include "fmt/format.h"

#include "vulkan_utility.h"

void vk_throw_impl(VkResult error_code, const std::string_view& api_call_name, const std::string_view& file, int line)
{
    throw std::runtime_error(fmt::format(
        "operation {} failed:\n"
            "\terror code: {}\n"
            "\tat:{}:{}\n",
        api_call_name,
        VulkanUtility::vk_result_to_string(error_code),
        file, line
        ));
}

void vk_expect(VkResult actual, VkResult expected, const std::string_view& api_call_name,
    const std::string_view& file, int line)
{
    [[unlikely]]
    if (expected != actual)
    {
        throw std::runtime_error(fmt::format(
            "operation {} failed:\n"
                "\texpected: {}\n"
                "\tactual: {}\n"
                "\tat:{}:{}\n",
            api_call_name,
            VulkanUtility::vk_result_to_string(expected),
            VulkanUtility::vk_result_to_string(actual),
            file, line
            ));
    }
}