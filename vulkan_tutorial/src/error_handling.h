#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#include "fmt/format.h"
#include "vulkan/vulkan.h"

#include "macro.h"
#include "vulkan_utility.h"

template<typename Exception = std::runtime_error>
void vk_expect(VkResult actual, VkResult expected, const std::string_view& api_call_name,
    const std::string_view& file, int line)
{
    [[unlikely]]
    if (expected != actual)
    {
        throw Exception(fmt::format(
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

// wraps vulkan call to throw an exception if function didn't return VK_SUCCESS
#define vk_wrap(function_name) [](auto&&... args)                                                       \
    {                                                                                                   \
        constexpr VkResult expected = VK_SUCCESS;                                                       \
        const VkResult actual = function_name(args...);                                                 \
        vk_expect(actual, expected, TOSTRING(function_name), __FILE__, __LINE__);                       \
    }
