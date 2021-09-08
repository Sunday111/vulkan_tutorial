#pragma once

#include <string_view>

#include "macro.h"
#include "vulkan/vulkan.h"

void VkThrowImpl(VkResult error_code, const std::string_view& api_call_name,
                 const std::string_view& file, int line);
void VkExpect(VkResult actual, VkResult expected,
              const std::string_view& api_call_name,
              const std::string_view& file, int line);

// wraps vulkan call to throw an exception if function didn't return VK_SUCCESS
#define VkWrap(function_name)                                             \
  [](auto&&... args) {                                                    \
    VkExpect(function_name(args...), VK_SUCCESS, TOSTRING(function_name), \
             __FILE__, __LINE__);                                         \
  }

#define VkThrow(function, error_code) \
  VkThrowImpl(error_code, TOSTRING(function_name), __FILE__, __LINE__)
