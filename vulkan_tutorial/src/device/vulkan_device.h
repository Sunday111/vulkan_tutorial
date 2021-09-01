#pragma once

#include "integer.h"
#include "vulkan/vulkan.h"

class VulkanDevice
{
public:
    VulkanDevice(VkDevice device = nullptr);
    VulkanDevice(VulkanDevice&&);
    VulkanDevice(const VulkanDevice&) = delete;
    ~VulkanDevice();

    VulkanDevice& operator=(VulkanDevice&& another);
    VulkanDevice& operator=(const VulkanDevice& another) = delete;

    VkQueue get_queue(ui32 family, ui32 queue) const noexcept;

    void destroy();
    void move_from(VulkanDevice& another);

private:
    VkDevice device_ = nullptr;
};
