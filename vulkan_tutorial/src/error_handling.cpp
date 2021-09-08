#include "error_handling.h"

#include <stdexcept>

#include "fmt/format.h"
#include "vulkan_utility.h"

void VkThrowImpl(VkResult error_code, const std::string_view& api_call_name,
                 const std::string_view& file, int line) {
  throw std::runtime_error(fmt::format(
      "operation {} failed:\n"
      "\terror code: {}\n"
      "\tat:{}:{}\n",
      api_call_name, VulkanUtility::ResultToString(error_code), file, line));
}

void VkExpect(VkResult actual, VkResult expected,
              const std::string_view& api_call_name,
              const std::string_view& file, int line) {
  [[unlikely]] if (expected != actual) {
    throw std::runtime_error(
        fmt::format("operation {} failed:\n"
                    "\texpected: {}\n"
                    "\tactual: {}\n"
                    "\tat:{}:{}\n",
                    api_call_name, VulkanUtility::ResultToString(expected),
                    VulkanUtility::ResultToString(actual), file, line));
  }
}