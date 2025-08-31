#include "input.h"

using namespace eng::platform;

static InputState g_state;

void Input::attach(GLFWwindow* win) {
    glfwSetKeyCallback(win, keyCb);
    glfwSetCursorPosCallback(win, cursorCb);
    glfwSetMouseButtonCallback(win, mouseCb);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

InputState& Input::state() { return g_state; }

void Input::keyCb(GLFWwindow*, int key, int, int action, int) {
    if (key < 0 || key >= 512) return;
    if (action == GLFW_PRESS) g_state.keys[key] = true;
    else if (action == GLFW_RELEASE) g_state.keys[key] = false;
}

void Input::cursorCb(GLFWwindow*, double xpos, double ypos) {
    if (g_state.firstMouse) {
        g_state.lastX = xpos; g_state.lastY = ypos; g_state.firstMouse = false;
    }
    g_state.mouseDx = xpos - g_state.lastX;
    g_state.mouseDy = ypos - g_state.lastY;
    g_state.lastX = xpos; g_state.lastY = ypos;
}

void Input::mouseCb(GLFWwindow*, int button, int action, int) {
    if (button < 0 || button >= 8) return;
    if (action == GLFW_PRESS) g_state.mouseButtons[button] = true;
    else if (action == GLFW_RELEASE) g_state.mouseButtons[button] = false;
}

