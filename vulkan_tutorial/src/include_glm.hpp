#pragma once

// clang-format off
#ifdef __clang__
    #define include_glm_begin \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Wsign-conversion\"") \
        _Pragma("GCC diagnostic ignored \"-Wconversion\"") \
        static_assert(true, "")
#else
    #define include_glm_begin \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Wsign-conversion\"") \
        _Pragma("GCC diagnostic ignored \"-Wconversion\"") \
        _Pragma("GCC diagnostic ignored \"-Wduplicated-branches\"") \
        static_assert(true, "")
#endif

#define include_glm_end _Pragma("GCC diagnostic pop") static_assert(true, "")

// clang-format on

include_glm_begin;
#include "glm/glm.hpp"
include_glm_end;