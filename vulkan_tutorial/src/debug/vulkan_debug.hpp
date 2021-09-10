#pragma once

#include <string_view>

#include "definitions.hpp"
#include "integer.hpp"
#include "unused_var.hpp"
#include "vulkan/vulkan.h"
#include "vulkan_object_type_traits.hpp"

struct LabelColor {
  static constexpr LabelColor Red() {
    return LabelColor{1.0f, 0.0f, 0.0f, 1.0f};
  }
  static constexpr LabelColor Green() {
    return LabelColor{0.0f, 1.0f, 0.0f, 1.0f};
  }
  static constexpr LabelColor Blue() {
    return LabelColor{0.0f, 0.0f, 1.0f, 1.0f};
  }
  float r, g, b, a;
};

struct VkDebug {
  void Initialize(VkInstance instance) noexcept;

  void BeginLabel(VkQueue queue, const std::string_view& name,
                  const LabelColor& color = LabelColor::Green()) const noexcept;
  void BeginLabel(VkCommandBuffer buffer, const std::string_view& name,
                  const LabelColor& color = LabelColor::Green()) const noexcept;
  void EndLabel(VkQueue queue) const noexcept;
  void EndLabel(VkCommandBuffer buffer) const noexcept;
  auto ScopedLabel(auto queue_or_buffer, const std::string_view& name,
                   const LabelColor& color) const noexcept {
    BeginLabel(queue_or_buffer, name, color);
    struct Deleter {
      const VkDebug* this_;
      decltype(queue_or_buffer) val;

      ~Deleter() { this_->EndLabel(val); }
    };

    return Deleter{this, queue_or_buffer};
  }

  [[nodiscard]] static VkDebugUtilsLabelEXT ConstructLabel(
      const std::string_view& name, const LabelColor& color) noexcept;

  template <typename T>
  void SetObjectName(VkDevice device, T handle,
                     const std::string_view& name) const noexcept {
    constexpr VkObjectType ObjectType = VulkanObjectTypeTraits<T>::Value;
    static_assert(sizeof(T) <= sizeof(ui64));
    SetObjectName(device, name, ObjectType, reinterpret_cast<ui64>(handle));
  }

  void SetObjectName(VkDevice device, const std::string_view& name,
                     VkObjectType object_type,
                     ui64 object_handle) const noexcept;

 private:
  template <auto Member, typename... Args>
  void CallImplFn(Args&&... args) const noexcept {
    if constexpr (kEnableDebugUtilsExtension) {
      (this->*Member)(std::forward<Args>(args)...);
    } else {
      UnusedVar(std::forward<Args>(args)...);
    }
  }

 private:
  PFN_vkQueueBeginDebugUtilsLabelEXT queue_begin_debug_utils_label_ = nullptr;
  PFN_vkQueueEndDebugUtilsLabelEXT queue_end_debug_utils_label_ = nullptr;
  PFN_vkCmdBeginDebugUtilsLabelEXT cmd_begin_debug_utils_label_ = nullptr;
  PFN_vkCmdEndDebugUtilsLabelEXT cmd_end_debug_utils_label_ = nullptr;
  PFN_vkCmdInsertDebugUtilsLabelEXT cmd_insert_debug_utils_label_ext_ = nullptr;
  PFN_vkSetDebugUtilsObjectNameEXT set_debug_utils_object_name_ = nullptr;
};
