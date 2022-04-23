cmake_minimum_required(VERSION 3.18)

if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif (POLICY CMP0048)

project (vulkan_tutorial)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED FATAL_ERROR)

# prints all cmake variables
#get_cmake_property(_variableNames VARIABLES)
#list (SORT _variableNames)
#foreach (_variableName ${_variableNames})
#    message("${_variableName}=${${_variableName}}")
#endforeach()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dep/fmt)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dep/spdlog)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dep/glfw)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dep/glm)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dep/stb)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dep/tinyobjloader)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vulkan_tutorial)

