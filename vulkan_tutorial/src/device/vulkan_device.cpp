#include "device/vulkan_device.h"


VulkanDevice::VulkanDevice(VkDevice device) noexcept
    : device_(device)
{
}

VulkanDevice::VulkanDevice(VulkanDevice&& another) noexcept
{
    move_from(another);
}

VulkanDevice::~VulkanDevice() noexcept
{
    destroy();
}

VulkanDevice& VulkanDevice::operator=(VulkanDevice&& another) noexcept
{
    destroy();
    move_from(another);
    return *this;
}

void VulkanDevice::destroy() noexcept
{
    if (device_)
    {
        vkDestroyDevice(device_, nullptr);
        device_ = nullptr;
    }
}

VkQueue VulkanDevice::get_queue(ui32 family, ui32 queue) const noexcept
{
    VkQueue out_queue;
    vkGetDeviceQueue(device_, family, queue, &out_queue);
    return out_queue;
}

void VulkanDevice::move_from(VulkanDevice& another) noexcept
{
    device_ = another.device_;
    another.device_ = nullptr;
}
