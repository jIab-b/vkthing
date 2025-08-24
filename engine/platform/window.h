#pragma once
#include <string>
#include <functional>
struct GLFWwindow;

namespace eng::platform {
    struct WindowCreateInfo {
        int width = 1280;
        int height = 720;
        std::string title = "Sandbox";
    };

    class Window {
    public:
        explicit Window(const WindowCreateInfo& ci);
        ~Window();
        Window(const Window&) = delete; Window& operator=(const Window&) = delete;

        bool shouldClose() const;
        void pollEvents();
        void getFramebufferSize(int& w, int& h) const;
        GLFWwindow* handle() const { return window_; }
    private:
        GLFWwindow* window_ = nullptr;
    };
}

