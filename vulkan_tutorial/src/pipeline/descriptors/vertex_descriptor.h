#pragma once

#include <array>

#include "vulkan/vulkan.h"

#include "struct_descriptor.h"
#include "pipeline/vertex.h"

template<>
struct StructDescriptor<Vertex>
{
    [[nodiscard]]
    static constexpr std::array<VkVertexInputBindingDescription, 1> get_binding_description() noexcept
    {
        VkVertexInputBindingDescription binding_description{};

        binding_description.binding = 0;
        binding_description.stride = sizeof(Vertex);
        binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return std::array{ binding_description };
    }

    [[nodiscard]]
    static constexpr std::array<VkVertexInputAttributeDescription, 2> get_input_attribute_descriptions() noexcept
    {
        VkVertexInputAttributeDescription d0{};
        d0.binding = 0;
        d0.location = 0;
        d0.format = VK_FORMAT_R32G32_SFLOAT;
        d0.offset = offsetof(Vertex, pos);

        VkVertexInputAttributeDescription d1{};
        d1.binding = 0;
        d1.location = 1;
        d1.format = VK_FORMAT_R32G32B32_SFLOAT;
        d1.offset = offsetof(Vertex, color);

        return std::array{d0, d1};
    }
};
