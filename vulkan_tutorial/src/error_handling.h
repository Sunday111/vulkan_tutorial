#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#include "fmt/format.h"
#include "vulkan/vulkan.h"

#include "macro.h"
#include "vulkan_utility.h"

template
<
    typename Exception = std::runtime_error,
    typename... Args
>
void vk_expect(VkResult actual, VkResult expected, const std::string_view& description_format, Args&&... args)
{
    [[unlikely]]
    if (expected != actual)
    {
        const auto expected_str = VulkanUtility::vk_result_to_string(expected);
        const auto actual_str = VulkanUtility::vk_result_to_string(actual);
        const auto description = fmt::format(description_format, std::forward<Args>(args)...);
        auto message = fmt::format(
            "operation failed:\n"
                "\texpected: {}\n"
                "\tactual: {}\n"
                "\tdescription: {}\n",
            expected_str,
            actual_str,
            description);
        throw Exception(std::move(message));
    }
}

template
<
    typename Exception = std::runtime_error,
    typename... Args
>
void vk_expect_success(VkResult result, const std::string_view& description_format, Args&&... args)
{
    vk_expect<Exception>(result, VK_SUCCESS, description_format, std::forward<Args>(args)...);
}

// wraps vulkan call to throw an exception if function didn't return VK_SUCCESS
#define vk_wrap(function_name) [](auto&&... args) { vk_expect_success(function_name(args...), TOSTRING(function_name)" at {}", __LINE__); }