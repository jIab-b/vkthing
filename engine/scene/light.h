#pragma once
#include <glm/glm.hpp>

namespace eng::scene {
    struct DirectionalLight {
        glm::vec3 direction{ -0.5f, -1.0f, -0.25f };
        glm::vec3 color{ 1.0f, 0.98f, 0.9f };
        float intensity = 3.0f;
    };
}

