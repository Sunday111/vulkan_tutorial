cmake_minimum_required(VERSION 3.20)

set(target_name vulkan_tutorial)

set(target_src_root ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(target_shaders_root ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(target_textures_root ${CMAKE_CURRENT_SOURCE_DIR}/textures)

file(GLOB_RECURSE sources_list "${target_src_root}/*.cpp")
file(GLOB_RECURSE shaders_list RELATIVE ${target_shaders_root} "${target_shaders_root}/**")
file(GLOB_RECURSE textures_list RELATIVE ${target_textures_root} "${target_textures_root}/**" )

add_executable(${target_name} ${sources_list})
target_link_libraries(${target_name} glfw fmt stb glm spdlog Vulkan::Vulkan)
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

# compile shaders
add_custom_target(compile_shaders)
foreach(shader_src_rel ${shaders_list})
	set(shader_src_abs ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader_src_rel})
	get_filename_component(shader_src_name ${shader_src_rel} NAME_WE)
	get_filename_component(shader_src_dir ${shader_src_rel} DIRECTORY)
	set(shader_dst_abs ${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader_src_dir}/${shader_src_name}.spv)

	# create desctination directory
	get_filename_component(shader_dst_dir ${shader_dst_abs} DIRECTORY)
	file(MAKE_DIRECTORY ${shader_dst_dir})

	add_custom_command(
		TARGET compile_shaders
		COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${shader_src_abs} -o ${shader_dst_abs}
		MAIN_DEPENDENCY ${shader_dst_abs}
	)
endforeach()

# copy textures
add_custom_target(copy_textures)
foreach(texture_src_rel ${textures_list})
	set(texture_src_abs ${target_textures_root}/${texture_src_rel})
	set(texture_dst_abs ${CMAKE_CURRENT_BINARY_DIR}/textures/${texture_src_rel})

	# create desctination directory
	get_filename_component(texture_dst_dir ${texture_dst_abs} DIRECTORY)
	file(MAKE_DIRECTORY ${texture_dst_dir})

	add_custom_command(
		TARGET copy_textures
		COMMAND ${CMAKE_COMMAND} -E copy ${texture_src_abs} ${texture_dst_abs}
		MAIN_DEPENDENCY ${texture_src})
endforeach()

add_dependencies(${target_name} compile_shaders copy_textures)
