#pragma once

#include "vulkan/vulkan.h"

#include <string_view>

struct LabelColor
{
    static constexpr LabelColor Red() { return LabelColor{1.0f, 0.0f, 0.0f, 1.0f}; }
    static constexpr LabelColor Green() { return LabelColor{0.0f, 1.0f, 0.0f, 1.0f}; }
    static constexpr LabelColor Blue() { return LabelColor{0.0f, 0.0f, 1.0f, 1.0f}; }
    float r, g, b, a;
};

struct VkAnnotate
{
    PFN_vkQueueBeginDebugUtilsLabelEXT pfnQueueBeginDebugUtilsLabelEXT = nullptr;
    PFN_vkQueueEndDebugUtilsLabelEXT pfnQueueEndDebugUtilsLabelEXT = nullptr;

    PFN_vkCmdBeginDebugUtilsLabelEXT pfnCmdBeginDebugUtilsLabelEXT = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT pfnCmdEndDebugUtilsLabelEXT = nullptr;

    PFN_vkCmdInsertDebugUtilsLabelEXT pfnCmdInsertDebugUtilsLabelEXT = nullptr;

    void Initialize(VkInstance instance) noexcept
    {
        pfnQueueBeginDebugUtilsLabelEXT = (PFN_vkQueueBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkQueueBeginDebugUtilsLabelEXT");
        pfnQueueEndDebugUtilsLabelEXT = (PFN_vkQueueEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkQueueEndDebugUtilsLabelEXT");
        pfnCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT");
        pfnCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT");
        pfnCmdInsertDebugUtilsLabelEXT = (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdInsertDebugUtilsLabelEXT");
    }

    void BeginLabel(VkQueue queue, const std::string_view& name, const LabelColor& color = LabelColor::Green()) const noexcept
    {
        const VkDebugUtilsLabelEXT label = ConstructLabel(name, color);
        pfnQueueBeginDebugUtilsLabelEXT(queue, &label);
    }

    void BeginLabel(VkCommandBuffer buffer, const std::string_view& name, const LabelColor& color = LabelColor::Green()) const noexcept
    {
        const VkDebugUtilsLabelEXT label = ConstructLabel(name, color);
        pfnCmdBeginDebugUtilsLabelEXT(buffer, &label);
    }

    void EndLabel(VkQueue queue) const noexcept
    {
        pfnQueueEndDebugUtilsLabelEXT(queue);
    }

    void EndLabel(VkCommandBuffer buffer) const noexcept
    {
        pfnCmdEndDebugUtilsLabelEXT(buffer);
    }

    auto ScopedLabel(auto queue_or_buffer, const std::string_view& name, const LabelColor& color) const noexcept
    {
        BeginLabel(queue_or_buffer, name, color);
        struct Deleter
        {
            const VkAnnotate* this_;
            decltype(queue_or_buffer) val;

            ~Deleter()
            {
                this_->EndLabel(val);
            }
        };

        return Deleter{this, queue_or_buffer};
    }

    [[nodiscard]]
    static VkDebugUtilsLabelEXT ConstructLabel(const std::string_view& name, const LabelColor& color) noexcept
    {
        VkDebugUtilsLabelEXT label{};
        label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        label.pLabelName = name.data();
        label.color[0] = color.r;
        label.color[1] = color.g;
        label.color[2] = color.b;
        label.color[3] = color.a;
        return label;
    }
};
