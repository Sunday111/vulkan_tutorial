#pragma once

#include <array>

#include "pipeline/vertex.hpp"
#include "struct_descriptor.hpp"
#include "vulkan/vulkan.h"

template <>
struct StructDescriptor<Vertex> {
  [[nodiscard]] static constexpr std::array<VkVertexInputBindingDescription, 1>
  GetBindingDescription() noexcept {
    VkVertexInputBindingDescription binding_description{};
    binding_description.binding = 0;
    binding_description.stride = sizeof(Vertex);
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return std::array{binding_description};
  }

  [[nodiscard]] static constexpr std::array<VkVertexInputAttributeDescription,
                                            3>
  GetInputAttributeDescriptions() noexcept {
    VkVertexInputAttributeDescription d0{};
    d0.binding = 0;
    d0.location = 0;
    d0.format = VK_FORMAT_R32G32B32_SFLOAT;
    d0.offset = offsetof(Vertex, pos);

    VkVertexInputAttributeDescription d1{};
    d1.binding = 0;
    d1.location = 1;
    d1.format = VK_FORMAT_R32G32B32_SFLOAT;
    d1.offset = offsetof(Vertex, color);

    VkVertexInputAttributeDescription d2{};
    d2.binding = 0;
    d2.location = 2;
    d2.format = VK_FORMAT_R32G32_SFLOAT;
    d2.offset = offsetof(Vertex, tex_coord);

    return std::array{d0, d1, d2};
  }
};
