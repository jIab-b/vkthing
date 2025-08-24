#pragma once
#include <GLFW/glfw3.h>

namespace eng::platform {
    struct InputState {
        double mouseDx = 0.0, mouseDy = 0.0;
        bool firstMouse = true;
        double lastX = 0.0, lastY = 0.0;
        bool keys[512]{};
    };

    class Input {
    public:
        static void attach(GLFWwindow* win);
        static InputState& state();
    private:
        static void keyCb(GLFWwindow*, int key, int, int action, int);
        static void cursorCb(GLFWwindow*, double xpos, double ypos);
    };
}

