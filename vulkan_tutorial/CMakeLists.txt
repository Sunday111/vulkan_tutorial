cmake_minimum_required(VERSION 3.20)

set(target_name vulkan_tutorial)

set(target_src_root ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(content_dir_rel content)
set(shaders_dir_rel ${content_dir_rel}/shaders)
set(textures_dir_rel ${content_dir_rel}/textures)
set(models_dir_rel ${content_dir_rel}/models)

set(src_content_dir  ${CMAKE_CURRENT_SOURCE_DIR}/${content_dir_rel})
set(src_shaders_dir  ${CMAKE_CURRENT_SOURCE_DIR}/${shaders_dir_rel})
set(src_textures_dir ${CMAKE_CURRENT_SOURCE_DIR}/${textures_dir_rel})
set(src_models_dir ${CMAKE_CURRENT_SOURCE_DIR}/${models_dir_rel})

set(dst_content_dir  ${CMAKE_CURRENT_BINARY_DIR}/${content_dir_rel})
set(dst_shaders_dir  ${CMAKE_CURRENT_BINARY_DIR}/${shaders_dir_rel})
set(dst_textures_dir ${CMAKE_CURRENT_BINARY_DIR}/${textures_dir_rel})
set(dst_models_dir ${CMAKE_CURRENT_BINARY_DIR}/${models_dir_rel})

file(GLOB_RECURSE sources_list "${target_src_root}/*.cpp")

add_executable(${target_name} ${sources_list})
target_link_libraries(${target_name}
	glfw
	fmt
	stb
	glm
	spdlog
	tinyobjloader
	Vulkan::Vulkan)
target_compile_definitions(${target_name} PUBLIC
	-DGLFW_INCLUDE_VULKAN
	-DGLM_FORCE_RADIANS
	-DGLM_FORCE_DEPTH_ZERO_TO_ONE
	-DTINYOBJLOADER_IMPLEMENTATION)
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
add_dependencies(${target_name} compile_shaders)
file(GLOB_RECURSE shaders_list RELATIVE ${src_shaders_dir} "${src_shaders_dir}/**")
foreach(shader_src_rel ${shaders_list})
	set(shader_src_abs ${src_shaders_dir}/${shader_src_rel})
	get_filename_component(shader_src_name ${shader_src_rel} NAME_WE)
	get_filename_component(shader_src_dir ${shader_src_rel} DIRECTORY)
	set(shader_dst_abs ${dst_shaders_dir}/${shader_src_dir}/${shader_src_name}.spv)

	# create desctination directory
	get_filename_component(shader_dst_dir ${shader_dst_abs} DIRECTORY)
	file(MAKE_DIRECTORY ${shader_dst_dir})

	add_custom_command(
		TARGET compile_shaders
		COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${shader_src_abs} -o ${shader_dst_abs}
		MAIN_DEPENDENCY ${shader_dst_abs}
	)
endforeach()

function(copy_content_fn)
	set(options)
	set(oneValueArgs TARGET SRC DST)
	set(multiValueArgs)
	cmake_parse_arguments(copy_content_fn "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	# gather source file paths relative to root
	file(GLOB_RECURSE files_list RELATIVE ${copy_content_fn_SRC} "${copy_content_fn_SRC}/**")

	add_custom_target(${copy_content_fn_TARGET})

	# generate copy commands for each file
	foreach(file_src_rel ${files_list})
		set(file_src_abs ${copy_content_fn_SRC}/${file_src_rel})
		set(file_dst_abs ${copy_content_fn_DST}/${file_src_rel})

		# create desctination directory
		get_filename_component(file_dst_dir ${file_dst_abs} DIRECTORY)
		file(MAKE_DIRECTORY ${file_dst_dir})

		add_custom_command(
			TARGET ${copy_content_fn_TARGET}
			COMMAND ${CMAKE_COMMAND} -E copy ${file_src_abs} ${file_dst_abs}
			MAIN_DEPENDENCY ${file_src_abs})
	endforeach()
endfunction()

add_custom_target(copy_content)
add_dependencies(${target_name} copy_content)

copy_content_fn(TARGET copy_textures SRC ${src_textures_dir} DST ${dst_textures_dir})
add_dependencies(copy_content copy_textures)

copy_content_fn(TARGET copy_models SRC ${src_models_dir} DST ${dst_models_dir})
add_dependencies(copy_content copy_models)

