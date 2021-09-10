#pragma once

// clang-format off
#define include_glm_begin \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wsign-conversion\"") \
    _Pragma("GCC diagnostic ignored \"-Wconversion\"") \
    _Pragma("GCC diagnostic ignored \"-Wduplicated-branches\"") \
    static_assert(true, "")

#define include_glm_end _Pragma("GCC diagnostic pop") static_assert(true, "")

// clang-format on