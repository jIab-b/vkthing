#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace eng::scene {
    struct Transform {
        glm::vec3 position{0.0f};
        glm::vec3 rotationEuler{0.0f}; // yaw (y), pitch (x), roll (z)
        glm::vec3 scale{1.0f};
        glm::mat4 matrix() const {
            glm::mat4 m(1.0f);
            m = glm::translate(m, position);
            m = glm::rotate(m, rotationEuler.y, glm::vec3(0,1,0));
            m = glm::rotate(m, rotationEuler.x, glm::vec3(1,0,0));
            m = glm::rotate(m, rotationEuler.z, glm::vec3(0,0,1));
            m = glm::scale(m, scale);
            return m;
        }
    };
}

