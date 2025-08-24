cmake_minimum_required(VERSION 3.16)
project(vkthing_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_SAMPLES "Build sample apps" ON)

find_package(Vulkan REQUIRED)
add_subdirectory(engine)
add_subdirectory(app/sandbox)

if(BUILD_SAMPLES)
  add_subdirectory(samples/vulkan_minimal)
  add_subdirectory(samples/vulkan_validation)
  add_subdirectory(samples/vulkan_window)
endif()
