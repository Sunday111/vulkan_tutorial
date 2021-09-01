#pragma once

#include <string>
#include <stdexcept>

#include "fmt/format.h"
#include "vulkan/vulkan.h"

std::string vk_result_to_string(VkResult vk_result);

template<typename... Args>
void vk_expect_success(VkResult result, const std::string_view& description_format, Args&&... args)
{
	[[unlikely]]
	if (result != VK_SUCCESS)
	{
		auto description = fmt::format(description_format, std::forward<Args>(args)...);
		auto message = fmt::format("operation failed:\n\tdescription: {}\n\terror: {}\n",
			description, vk_result_to_string(result));
		throw std::runtime_error(std::move(message));
	}
}
