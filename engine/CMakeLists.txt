add_library(engine_core INTERFACE)
target_include_directories(engine_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# GLFW dependency (Linux via pkg-config; Windows via CMake config/vcpkg)
set(GLFW_INCLUDE_DIRS "")
set(GLFW_LINK_LIBRARIES "")
if (WIN32)
  # If caller provided both include and link variables (non-empty), respect and skip discovery.
  if (DEFINED GLFW_INCLUDE_DIRS AND DEFINED GLFW_LINK_LIBRARIES AND GLFW_INCLUDE_DIRS AND GLFW_LINK_LIBRARIES)
    message(STATUS "Using user-provided GLFW include=${GLFW_INCLUDE_DIRS} lib=${GLFW_LINK_LIBRARIES}")
  else()
    # Prefer CMake package (e.g., via vcpkg)
    find_package(glfw3 CONFIG QUIET)
    if (TARGET glfw)
      set(GLFW_LINK_LIBRARIES glfw)
    elseif (TARGET glfw3)
      set(GLFW_LINK_LIBRARIES glfw3)
    elseif (TARGET glfw3::glfw)
      set(GLFW_LINK_LIBRARIES glfw3::glfw)
    else()
      # Fallback: allow manual include/lib discovery via environment hints (GLFW_DIR/GLFW_PATH/GFLW_PATH)
      # This expects a classic layout: <root>/include/GLFW/glfw3.h and <root>/lib{,64}/glfw3.lib|glfw3dll.lib
      # Common default install under Program Files (x86 variant omitted to avoid CMake env var parsing issues)
      set(_GLFW_DEFAULT_HINTS "$ENV{ProgramFiles}/glfw")
      find_path(GLFW_INCLUDE_DIRS NAMES GLFW/glfw3.h
        HINTS
          ${_GLFW_DEFAULT_HINTS}
          $ENV{glfw3_DIR} $ENV{GLFW_DIR} $ENV{GLFW_ROOT} $ENV{GLFW_PATH} $ENV{GFLW_PATH}
        PATH_SUFFIXES include
      )
      # Explicit last-resort defaults under Program Files
      if (NOT GLFW_INCLUDE_DIRS AND DEFINED ENV{ProgramFiles})
        set(_GLFW_INC "$ENV{ProgramFiles}/glfw/include")
        if (EXISTS "${_GLFW_INC}/GLFW/glfw3.h")
          set(GLFW_INCLUDE_DIRS "${_GLFW_INC}")
        endif()
      endif()
      # If include found, prefer library search relative to that root
      if (GLFW_INCLUDE_DIRS)
        get_filename_component(_GLFW_ROOT "${GLFW_INCLUDE_DIRS}" DIRECTORY)
      endif()
      find_library(GLFW_LIB
        NAMES glfw3 glfw3dll glfw
        HINTS
          ${_GLFW_ROOT}
          ${_GLFW_DEFAULT_HINTS}
          $ENV{glfw3_DIR} $ENV{GLFW_DIR} $ENV{GLFW_ROOT} $ENV{GLFW_PATH} $ENV{GFLW_PATH}
        PATH_SUFFIXES
          lib lib64
          lib-vc2022 lib-vc2019 lib-vc2017 lib-vc2015 lib-vc2013
          lib-static-ucrt lib-mingw-w64
      )
      # As a last resort try direct known library locations when Program Files hint is available
      if (NOT GLFW_LIB AND DEFINED ENV{ProgramFiles})
        set(_GLFW_PF "$ENV{ProgramFiles}/glfw")
        foreach(sd lib-vc2022 lib-vc2019 lib-vc2017 lib-vc2015 lib lib64 lib-static-ucrt lib-mingw-w64)
          if (EXISTS "${_GLFW_PF}/${sd}/glfw3.lib")
            set(GLFW_LIB "${_GLFW_PF}/${sd}/glfw3.lib")
            break()
          elseif (EXISTS "${_GLFW_PF}/${sd}/glfw3dll.lib")
            set(GLFW_LIB "${_GLFW_PF}/${sd}/glfw3dll.lib")
            break()
          endif()
        endforeach()
      endif()

      if (GLFW_INCLUDE_DIRS AND GLFW_LIB)
        set(GLFW_LINK_LIBRARIES ${GLFW_LIB})
        message(STATUS "Using GLFW from includes: ${GLFW_INCLUDE_DIRS}, lib: ${GLFW_LIB}")
      else()
        message(FATAL_ERROR "GLFW not found. Install via vcpkg (vcpkg install glfw3) or set GLFW_DIR/GLFW_PATH to a root with include+lib.")
      endif()
    endif()
  endif()
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GLFW REQUIRED glfw3)
  set(GLFW_INCLUDE_DIRS ${GLFW_INCLUDE_DIRS})
  set(GLFW_LINK_LIBRARIES ${GLFW_LINK_LIBRARIES})
endif()

# tinygltf dependency from third_party - MOVED UP
if (DEFINED THIRD_PARTY_DIR)
  set(TINYGLTF_INCLUDE_DIR "${THIRD_PARTY_DIR}/tinygltf")
  if (EXISTS "${TINYGLTF_INCLUDE_DIR}/tiny_gltf.h")
    add_library(tinygltf INTERFACE)
    target_include_directories(tinygltf INTERFACE ${TINYGLTF_INCLUDE_DIR})
    # TINYGLTF_IMPLEMENTATION will be defined in main.cpp to avoid multiple definitions
    message(STATUS "Using tinygltf from third_party: ${TINYGLTF_INCLUDE_DIR}")
  endif()
endif()

add_library(engine_platform
  platform/window.cpp
  platform/window.h
  platform/input.cpp
  platform/input.h
)
target_include_directories(engine_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${GLFW_INCLUDE_DIRS})
target_link_libraries(engine_platform PUBLIC ${GLFW_LINK_LIBRARIES})

add_library(engine_scene
  scene/gltf_loader.cpp
  scene/gltf_loader.h
)
target_include_directories(engine_scene PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(engine_scene PUBLIC tinygltf glm::glm)

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
if (TARGET tinygltf)
  target_link_libraries(engine_renderer_vk PUBLIC tinygltf)
endif()

add_library(engine_terrain
  terrain/terrain.h
  terrain/terrain.cpp
)
target_include_directories(engine_terrain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if (TARGET tinygltf)
  target_link_libraries(engine_terrain PUBLIC tinygltf)
endif()

# GLM dependency: prefer package, fallback to header-only include path
set(_glm_ok FALSE)
find_package(glm QUIET)
if (TARGET glm::glm)
  target_link_libraries(engine_terrain PUBLIC glm::glm)
  set(_glm_ok TRUE)
else()
  # Allow user/env to supply include path. Common hints: GLM_DIR, GLM_PATH, VULKAN_SDK/Include
  find_path(GLM_INCLUDE_DIR
    NAMES glm/glm.hpp
    HINTS $ENV{GLM_DIR} $ENV{GLM_PATH} "$ENV{VULKAN_SDK}/Include"
    PATH_SUFFIXES include
  )
  if (GLM_INCLUDE_DIR)
    add_library(glm_header_only INTERFACE)
    target_include_directories(glm_header_only INTERFACE ${GLM_INCLUDE_DIR})
    add_library(glm::glm ALIAS glm_header_only)
    target_link_libraries(engine_terrain PUBLIC glm::glm)
    message(STATUS "Using GLM headers from: ${GLM_INCLUDE_DIR}")
    set(_glm_ok TRUE)
  endif()
endif()
if (NOT _glm_ok)
  message(FATAL_ERROR "glm not found. Install via vcpkg (glm) or set GLM_DIR/GLM_PATH or VULKAN_SDK to a path containing include/glm.")
endif()



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
add_custom_command(
  OUTPUT ${COMPILED_SHADER_DIR}/mesh.vert.spv
  COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_DIR}/mesh.vert -o ${COMPILED_SHADER_DIR}/mesh.vert.spv
  DEPENDS ${SHADER_DIR}/mesh.vert
  COMMENT "Compiling mesh.vert"
)
add_custom_command(
  OUTPUT ${COMPILED_SHADER_DIR}/mesh.frag.spv
  COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_DIR}/mesh.frag -o ${COMPILED_SHADER_DIR}/mesh.frag.spv
  DEPENDS ${SHADER_DIR}/mesh.frag
  COMMENT "Compiling mesh.frag"
)
add_custom_target(shaders ALL DEPENDS
  ${COMPILED_SHADER_DIR}/terrain_points.vert.spv
  ${COMPILED_SHADER_DIR}/terrain_points.frag.spv
  ${COMPILED_SHADER_DIR}/mesh.vert.spv
  ${COMPILED_SHADER_DIR}/mesh.frag.spv
)

# Copy shaders next to sandbox executable as well
add_custom_command(TARGET shaders POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/app/sandbox/shaders
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${COMPILED_SHADER_DIR}/terrain_points.vert.spv
          ${COMPILED_SHADER_DIR}/terrain_points.frag.spv
          ${COMPILED_SHADER_DIR}/mesh.vert.spv
          ${COMPILED_SHADER_DIR}/mesh.frag.spv
          ${CMAKE_BINARY_DIR}/app/sandbox/shaders
)
