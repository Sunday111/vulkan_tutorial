#pragma once

#include <string_view>

#include "vulkan/vulkan.h"

#include "macro.h"

void vk_throw_impl(VkResult error_code, const std::string_view& api_call_name, const std::string_view& file, int line);
void vk_expect(VkResult actual, VkResult expected, const std::string_view& api_call_name, const std::string_view& file, int line);

// wraps vulkan call to throw an exception if function didn't return VK_SUCCESS
#define vk_wrap(function_name) [](auto&&... args) { \
        vk_expect(function_name(args...), VK_SUCCESS, TOSTRING(function_name), __FILE__, __LINE__); }

#define vk_throw(function, error_code) vk_throw_impl(error_code, TOSTRING(function_name), __FILE__, __LINE__)
