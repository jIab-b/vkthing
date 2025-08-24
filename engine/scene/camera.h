#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace eng::scene {
    struct Camera {
        glm::vec3 position{0,1.5f,5};
        float yaw = 0.0f;   // radians
        float pitch = 0.0f; // radians
        float fov = glm::radians(70.0f);
        float nearPlane = 0.1f;
        float farPlane = 2000.0f;

        glm::mat4 view() const {
            glm::vec3 fwd{
                cosf(pitch) * sinf(yaw),
                sinf(pitch),
                cosf(pitch) * cosf(yaw)
            };
            glm::vec3 right = glm::normalize(glm::cross(fwd, {0,1,0}));
            glm::vec3 up = glm::normalize(glm::cross(right, fwd));
            return glm::lookAt(position, position + fwd, up);
        }
        glm::mat4 proj(float aspect) const {
            glm::mat4 p = glm::perspective(fov, aspect, nearPlane, farPlane);
            p[1][1] *= -1.0f; // Vulkan-style
            return p;
        }
    };
}

