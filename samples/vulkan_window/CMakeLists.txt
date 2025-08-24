cmake_minimum_required(VERSION 3.16)
project(vulkan_window CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)

add_executable(vulkan_window main.cpp)
target_include_directories(vulkan_window PRIVATE ${GLFW_INCLUDE_DIRS})
target_link_libraries(vulkan_window PRIVATE Vulkan::Vulkan ${GLFW_LINK_LIBRARIES})

