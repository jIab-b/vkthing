cmake_minimum_required(VERSION 3.16)
project(vulkan_validation CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

add_executable(vulkan_validation main.cpp)
target_link_libraries(vulkan_validation PRIVATE Vulkan::Vulkan)

