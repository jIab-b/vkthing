project(sandbox CXX)

add_executable(sandbox
  main.cpp
)
target_include_directories(sandbox PRIVATE ${CMAKE_SOURCE_DIR})
add_dependencies(sandbox shaders)
target_link_libraries(sandbox PRIVATE engine_core engine_platform engine_scene engine_ecs engine_renderer engine_renderer_vk engine_terrain)

# Copy shaders next to the executable (handles multi-config generators on Windows)
add_custom_command(TARGET sandbox POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:sandbox>/shaders
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_BINARY_DIR}/shaders/terrain_points.vert.spv
          ${CMAKE_BINARY_DIR}/shaders/terrain_points.frag.spv
          $<TARGET_FILE_DIR:sandbox>/shaders
)
