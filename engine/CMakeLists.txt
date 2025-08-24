add_library(engine_core INTERFACE)
target_include_directories(engine_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# GLFW dependency (Linux via pkg-config; Windows via CMake config/vcpkg)
set(GLFW_INCLUDE_DIRS "")
set(GLFW_LINK_LIBRARIES "")
if (WIN32)
  find_package(glfw3 CONFIG QUIET)
  if (TARGET glfw)
    set(GLFW_LINK_LIBRARIES glfw)
  elseif (TARGET glfw3)
    set(GLFW_LINK_LIBRARIES glfw3)
  elseif (TARGET glfw3::glfw)
    set(GLFW_LINK_LIBRARIES glfw3::glfw)
  else()
    message(FATAL_ERROR "GLFW not found. Install via vcpkg: vcpkg install glfw3")
  endif()
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GLFW REQUIRED glfw3)
  set(GLFW_INCLUDE_DIRS ${GLFW_INCLUDE_DIRS})
  set(GLFW_LINK_LIBRARIES ${GLFW_LINK_LIBRARIES})
endif()

add_library(engine_platform
  platform/window.cpp
  platform/window.h
  platform/input.cpp
  platform/input.h
)
target_include_directories(engine_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${GLFW_INCLUDE_DIRS})
target_link_libraries(engine_platform PUBLIC ${GLFW_LINK_LIBRARIES})

add_library(engine_scene INTERFACE)
target_include_directories(engine_scene INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(engine_renderer INTERFACE)
target_include_directories(engine_renderer INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(engine_renderer INTERFACE Vulkan::Vulkan)

add_library(engine_ecs INTERFACE)
target_include_directories(engine_ecs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Vulkan renderer implementation
add_library(engine_renderer_vk
  renderer/vulkan_renderer.h
  renderer/vulkan_renderer.cpp
)
target_include_directories(engine_renderer_vk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${GLFW_INCLUDE_DIRS})
target_link_libraries(engine_renderer_vk PUBLIC Vulkan::Vulkan ${GLFW_LINK_LIBRARIES})

add_library(engine_terrain
  terrain/terrain.h
  terrain/terrain.cpp
)
target_include_directories(engine_terrain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
find_package(glm REQUIRED)
target_link_libraries(engine_terrain PUBLIC glm::glm)

# Shader compilation
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(COMPILED_SHADER_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${COMPILED_SHADER_DIR})

# Find glslangValidator (use Vulkan SDK on Windows if available)
if (WIN32 AND DEFINED ENV{VULKAN_SDK})
  find_program(GLSLANG_VALIDATOR NAMES glslangValidator HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/Bin32" REQUIRED)
else()
  find_program(GLSLANG_VALIDATOR NAMES glslangValidator REQUIRED)
endif()

add_custom_command(
  OUTPUT ${COMPILED_SHADER_DIR}/terrain_points.vert.spv
  COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_DIR}/terrain_points.vert -o ${COMPILED_SHADER_DIR}/terrain_points.vert.spv
  DEPENDS ${SHADER_DIR}/terrain_points.vert
  COMMENT "Compiling terrain_points.vert"
)
add_custom_command(
  OUTPUT ${COMPILED_SHADER_DIR}/terrain_points.frag.spv
  COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_DIR}/terrain_points.frag -o ${COMPILED_SHADER_DIR}/terrain_points.frag.spv
  DEPENDS ${SHADER_DIR}/terrain_points.frag
  COMMENT "Compiling terrain_points.frag"
)
add_custom_target(shaders ALL DEPENDS ${COMPILED_SHADER_DIR}/terrain_points.vert.spv ${COMPILED_SHADER_DIR}/terrain_points.frag.spv)

# Copy shaders next to sandbox executable as well
add_custom_command(TARGET shaders POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/app/sandbox/shaders
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${COMPILED_SHADER_DIR}/terrain_points.vert.spv
          ${COMPILED_SHADER_DIR}/terrain_points.frag.spv
          ${CMAKE_BINARY_DIR}/app/sandbox/shaders
)
