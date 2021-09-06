cmake_minimum_required(VERSION 3.20)

set(target_name vulkan_tutorial)

set(target_src_root ${CMAKE_CURRENT_SOURCE_DIR}/src/)
file(GLOB_RECURSE source_list "${target_src_root}/*.cpp")

set(target_shader_root ${CMAKE_CURRENT_SOURCE_DIR}/shaders/)
file(GLOB_RECURSE shader_list "${target_shader_root}/**")

add_executable(${target_name} ${source_list})
target_link_libraries(${target_name} glfw fmt glm spdlog Vulkan::Vulkan)
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

# compile shaders on pre build step
foreach(shader_file ${shader_list})
	get_filename_component(shader_name ${shader_file} NAME_WE)
	set(compiled_shader shaders/${shader_name}.spv)
	add_custom_command(
		TARGET ${target_name} PRE_BUILD
		COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${shader_file} -o ${compiled_shader}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/${compiled_shader}
	)
endforeach()
