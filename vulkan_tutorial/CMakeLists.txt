cmake_minimum_required(VERSION 3.20)
set(target_name vulkan_tutorial)
set(target_src_root ${CMAKE_CURRENT_SOURCE_DIR}/src/)
file(GLOB_RECURSE source_list "${target_src_root}/*.cpp")
message("sources: ${source_list}")
add_executable(${target_name} ${source_list})
target_link_libraries(${target_name} glfw fmt glm Vulkan::Vulkan)
target_compile_definitions(${target_name} PUBLIC
	-DGLFW_INCLUDE_VULKAN
	-DGLM_FORCE_RADIANS
	-DGLM_FORCE_DEPTH_ZERO_TO_ONE)
target_include_directories(${target_name} PUBLIC ${target_src_root})

if(MSVC)
  # Force to always compile with W4
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()
else()
  target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
