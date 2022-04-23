#pragma once

#include "include_glm.hpp"
include_glm_begin;
#include "glm/mat4x4.hpp"
#include "glm/vec4.hpp"
include_glm_end;

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 tex_coord;
};
