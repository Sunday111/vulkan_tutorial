#include "debug/vulkan_debug.hpp"

void VkDebug::Initialize(VkInstance instance) noexcept {
  if constexpr (kEnableDebugUtilsExtension) {
    queue_begin_debug_utils_label_ =
        reinterpret_cast<PFN_vkQueueBeginDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(instance, "vkQueueBeginDebugUtilsLabelEXT"));
    queue_end_debug_utils_label_ =
        reinterpret_cast<PFN_vkQueueEndDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(instance, "vkQueueEndDebugUtilsLabelEXT"));
    cmd_begin_debug_utils_label_ =
        reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT"));
    cmd_end_debug_utils_label_ =
        reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT"));
    cmd_insert_debug_utils_label_ext_ =
        reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(instance, "vkCmdInsertDebugUtilsLabelEXT"));
    set_debug_utils_object_name_ =
        reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
            vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));
  }
}

VkDebugUtilsLabelEXT VkDebug::ConstructLabel(const std::string_view& name,
                                             const LabelColor& color) noexcept {
  VkDebugUtilsLabelEXT label{};
  label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  label.pLabelName = name.data();
  label.color[0] = color.r;
  label.color[1] = color.g;
  label.color[2] = color.b;
  label.color[3] = color.a;
  return label;
}

void VkDebug::SetObjectName(VkDevice device, const std::string_view& name,
                            VkObjectType object_type,
                            ui64 object_handle) const noexcept {
  VkDebugUtilsObjectNameInfoEXT name_info{};
  name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
  name_info.pObjectName = name.data();
  name_info.objectType = object_type;
  name_info.objectHandle = object_handle;
  CallImplFn<&VkDebug::set_debug_utils_object_name_>(device, &name_info);
}

void VkDebug::BeginLabel(VkQueue queue, const std::string_view& name,
                         const LabelColor& color) const noexcept {
  const VkDebugUtilsLabelEXT label = ConstructLabel(name, color);
  CallImplFn<&VkDebug::queue_begin_debug_utils_label_>(queue, &label);
}

void VkDebug::BeginLabel(VkCommandBuffer buffer, const std::string_view& name,
                         const LabelColor& color) const noexcept {
  const VkDebugUtilsLabelEXT label = ConstructLabel(name, color);
  CallImplFn<&VkDebug::cmd_begin_debug_utils_label_>(buffer, &label);
}

void VkDebug::EndLabel(VkQueue queue) const noexcept {
  CallImplFn<&VkDebug::queue_end_debug_utils_label_>(queue);
}

void VkDebug::EndLabel(VkCommandBuffer buffer) const noexcept {
  CallImplFn<&VkDebug::cmd_end_debug_utils_label_>(buffer);
}