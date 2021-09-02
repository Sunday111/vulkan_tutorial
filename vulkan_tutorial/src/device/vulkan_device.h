#pragma once

#include "integer.h"
#include "vulkan/vulkan.h"

class VulkanDevice
{
public:
    VulkanDevice(VkDevice device = nullptr) noexcept;
    VulkanDevice(VulkanDevice&&) noexcept;
    VulkanDevice(const VulkanDevice&) = delete;
    ~VulkanDevice() noexcept;

    VulkanDevice& operator=(VulkanDevice&& another) noexcept;
    VulkanDevice& operator=(const VulkanDevice& another) = delete;

    VkQueue get_queue(ui32 family, ui32 queue) const noexcept;

    void destroy() noexcept;
    void move_from(VulkanDevice& another) noexcept;

    operator VkDevice() const noexcept { return device_; }

private:
    VkDevice device_ = nullptr;
};
