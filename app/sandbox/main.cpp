#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "engine/core/time.h"
#include "engine/core/log.h"
#include "engine/platform/window.h"
#include "engine/platform/input.h"
#include "engine/scene/camera.h"
#include "engine/renderer/vulkan_renderer.h"
#include "engine/scene/gltf_loader.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

using namespace eng;

// Path resolution utilities
std::string findProjectRoot() {
    fs::path current = fs::current_path();

    // Try to find the project root by looking for the "scenes" directory
    while (current != current.parent_path()) {
        if (fs::exists(current / "scenes") && fs::exists(current / "scenes" / "old_town")) {
            return current.string();
        }
        current = current.parent_path();
    }

    // Fallback: assume we're in the project root
    return fs::current_path().string();
}

std::string getGltfPath() {
    std::string root = findProjectRoot();
    return root + "/scenes/old_town/scene.gltf";
}

int main() {
    platform::WindowCreateInfo wci; wci.title = "Sandbox"; wci.width = 1280; wci.height = 720;
    platform::Window window(wci);
    platform::Input::attach(window.handle());

    scene::Camera cam;
    time::DeltaTimer timer;

    printf("Sandbox started. WASD + Mouse to move. ESC to quit.\n");

    // Check if window was created successfully
    if (!window.handle()) {
        printf("ERROR: Window creation failed\n");
        return 1;
    }
    printf("Window created successfully\n");

    printf("Creating VulkanRenderer object...\n");
    renderer::VulkanRenderer vk;
    printf("VulkanRenderer object created successfully\n");
    printf("Initializing Vulkan renderer...\n");
    if (!vk.initialize(window.handle())) {
        printf("ERROR: Failed to init Vulkan renderer\n");
        return 1;
    }
    printf("Vulkan renderer initialized successfully\n");

    // Load GLTF scene
    std::string gltfPath = getGltfPath();
    printf("Loading GLTF scene from: %s\n", gltfPath.c_str());
    try {
        auto gltfMeshes = eng::scene::GltfLoader::loadScene(gltfPath);
        printf("GLTF loading completed, got %zu meshes\n", gltfMeshes.size());

        if (!gltfMeshes.empty()) {
            // TEMPORARY FIX: Use known bounds from GLTF file instead of calculated bounds
            // This bypasses the bounds calculation issue for immediate testing
            printf("Using known scene bounds (temporary fix)...\n");
            eng::scene::SceneBounds sceneBounds;
            sceneBounds.min = glm::vec3(-41029.0f, -55588.0f, -20070.0f);
            sceneBounds.max = glm::vec3(86968.0f, 72227.0f, 36623.0f);
            sceneBounds.center = (sceneBounds.min + sceneBounds.max) * 0.5f;
            sceneBounds.radius = glm::length(sceneBounds.max - sceneBounds.center);

            printf("Known bounds: min(%.1f, %.1f, %.1f) max(%.1f, %.1f, %.1f) radius=%.1f\n",
                   sceneBounds.min.x, sceneBounds.min.y, sceneBounds.min.z,
                   sceneBounds.max.x, sceneBounds.max.y, sceneBounds.max.z,
                   sceneBounds.radius);

            printf("Adjusting camera for scene...\n");
            cam.adjustForScene(sceneBounds);

            // TODO: Remove this temporary fix once bounds calculation is working
            // auto sceneBounds = eng::scene::GltfLoader::getSceneBounds(gltfMeshes);

            // Load meshes into renderer
            printf("Loading meshes into Vulkan renderer...\n");
            vk.loadGltfMeshes(gltfMeshes);
            printf("Successfully loaded GLTF scene with %zu meshes\n", gltfMeshes.size());
        } else {
            printf("ERROR: No meshes loaded from GLTF scene\n");
        }
    } catch (const std::exception& e) {
        printf("ERROR: GLTF loading failed: %s\n", e.what());
    }

    while (!window.shouldClose()) {
        float dt = timer.tick();
        platform::InputState& in = platform::Input::state();

        const float speed = 100000.0f;
        const float sensitivity = 0.0025f;

        // Mouse look
        cam.yaw   -= static_cast<float>(in.mouseDx) * sensitivity;
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
        if (glfwGetMouseButton(window.handle(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) move.y -= 1.0f;
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
