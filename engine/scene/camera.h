#pragma once
#include <cfloat>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace eng::scene {

    // Forward declare SceneBounds
    struct SceneBounds {
        glm::vec3 min{FLT_MAX, FLT_MAX, FLT_MAX};
        glm::vec3 max{-FLT_MAX, -FLT_MAX, -FLT_MAX};
        glm::vec3 center{0.0f, 0.0f, 0.0f};
        float radius{0.0f};

        void update(const glm::vec3& point) {
            min = glm::min(min, point);
            max = glm::max(max, point);
        }

        void finalize() {
            center = (min + max) * 0.5f;
            radius = glm::length(max - center);
        }
    };

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

        // Adjust camera for optimal scene viewing
        void adjustForScene(const SceneBounds& bounds) {
            if (bounds.radius <= 0.0f) {
                printf("Warning: Invalid scene bounds (radius: %f)\n", bounds.radius);
                return;
            }

            // Position camera to view entire scene from a good angle
            position = bounds.center + glm::vec3(0.0f, bounds.radius * 0.5f, bounds.radius * 2.0f);

            // Adjust far plane based on scene size
            farPlane = bounds.radius * 4.0f;

            // Adjust near plane to prevent clipping
            nearPlane = bounds.radius * 0.001f;
            if (nearPlane < 0.01f) nearPlane = 0.01f;

            printf("Camera adjusted for scene:\n");
            printf("  Position: (%.1f, %.1f, %.1f)\n", position.x, position.y, position.z);
            printf("  Near plane: %.3f, Far plane: %.1f\n", nearPlane, farPlane);
            printf("  Scene center: (%.1f, %.1f, %.1f), radius: %.1f\n",
                   bounds.center.x, bounds.center.y, bounds.center.z, bounds.radius);
        }
    };
}

