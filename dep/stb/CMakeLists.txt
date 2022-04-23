cmake_minimum_required(VERSION 3.18)

set(target_name stb)
set(target_root ${CMAKE_CURRENT_SOURCE_DIR})
set(target_include_dir ${target_root})

add_library(${target_name} INTERFACE)
target_include_directories(${target_name} INTERFACE ${target_include_dir})
target_compile_definitions(${target_name} INTERFACE -DSTB_IMAGE_IMPLEMENTATION)
