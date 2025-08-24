#include "window.h"
#include <GLFW/glfw3.h>
#include <stdexcept>

using namespace eng::platform;

Window::Window(const WindowCreateInfo& ci) {
    if (!glfwInit()) throw std::runtime_error("GLFW init failed");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(ci.width, ci.height, ci.title.c_str(), nullptr, nullptr);
    if (!window_) throw std::runtime_error("GLFW window creation failed");
}

Window::~Window() {
    if (window_) glfwDestroyWindow(window_);
    glfwTerminate();
}

bool Window::shouldClose() const { return glfwWindowShouldClose(window_); }

void Window::pollEvents() { glfwPollEvents(); }

void Window::getFramebufferSize(int& w, int& h) const { glfwGetFramebufferSize(window_, &w, &h); }

