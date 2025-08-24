#include "engine/core/time.h"
#include "engine/core/log.h"
#include "engine/platform/window.h"
#include "engine/platform/input.h"
#include "engine/scene/camera.h"
#include "engine/renderer/vulkan_renderer.h"
#include <GLFW/glfw3.h>
#include <cmath>

using namespace eng;

int main() {
    platform::WindowCreateInfo wci; wci.title = "Sandbox"; wci.width = 1280; wci.height = 720;
    platform::Window window(wci);
    platform::Input::attach(window.handle());

    scene::Camera cam;
    time::DeltaTimer timer;

    eng::log::info("Sandbox started. WASD + Mouse to move. ESC to quit.");

    renderer::VulkanRenderer vk;
    if (!vk.initialize(window.handle())) {
        eng::log::error("Failed to init Vulkan renderer");
        return 1;
    }

    while (!window.shouldClose()) {
        float dt = timer.tick();
        platform::InputState& in = platform::Input::state();

        const float speed = 10.0f;
        const float sensitivity = 0.0025f;

        // Mouse look
        cam.yaw   += static_cast<float>(in.mouseDx) * sensitivity;
        cam.pitch -= static_cast<float>(in.mouseDy) * sensitivity;
        cam.pitch = std::clamp(cam.pitch, -1.55f, 1.55f);
        in.mouseDx = in.mouseDy = 0.0;

        // WASD
        glm::vec3 fwd{ cosf(cam.pitch) * sinf(cam.yaw), 0.0f, cosf(cam.pitch) * cosf(cam.yaw) };
        glm::vec3 right = glm::normalize(glm::cross(fwd, {0,1,0}));
        glm::vec3 move{0};
        if (in.keys[GLFW_KEY_W]) move += fwd;
        if (in.keys[GLFW_KEY_S]) move -= fwd;
        if (in.keys[GLFW_KEY_A]) move -= right;
        if (in.keys[GLFW_KEY_D]) move += right;
        if (in.keys[GLFW_KEY_SPACE]) move.y += 1.0f;
        if (in.keys[GLFW_KEY_LEFT_SHIFT]) move.y -= 1.0f;
        if (glm::length(move) > 0.0f) cam.position += glm::normalize(move) * speed * dt;

        if (in.keys[GLFW_KEY_ESCAPE]) glfwSetWindowShouldClose(window.handle(), 1);

        window.pollEvents();
        int fbw=0, fbh=0; window.getFramebufferSize(fbw, fbh);
        float aspect = fbh>0 ? (float)fbw/(float)fbh : 1.0f;
        glm::mat4 vp = cam.proj(aspect) * cam.view();
        vk.setVP(&vp[0][0]);
        vk.setPointSize(3.0f);
        const float lightDir[3] = {-0.5f, -1.0f, -0.25f};
        const float lightColor[3] = {1.0f, 0.98f, 0.9f};
        vk.setLight(lightDir, lightColor, 2.0f);
        vk.drawFrame(0.05f, 0.07f, 0.12f);
    }
    vk.shutdown();
    eng::log::info("Goodbye.");
    return 0;
}
