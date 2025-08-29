#pragma once
#include <vector>
#include <cfloat>
#include <glm/glm.hpp>
#include <tiny_gltf.h>

namespace eng::scene {

    // Forward declare SceneBounds (defined in camera.h)
    struct SceneBounds;

struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

struct Mesh {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    glm::mat4 transform;
};

// SceneBounds is defined in camera.h

class GltfLoader {
public:
    static std::vector<Mesh> loadScene(const std::string& gltfPath);
    static SceneBounds getSceneBounds(const std::vector<Mesh>& meshes);

private:
    static glm::mat4 getNodeTransform(const tinygltf::Node& node);
    static void processMesh(const tinygltf::Model& model,
                           const tinygltf::Mesh& mesh,
                           const glm::mat4& transform,
                           std::vector<Mesh>& outMeshes);
    static void extractMeshData(const tinygltf::Model& model,
                               const tinygltf::Primitive& primitive,
                               std::vector<MeshVertex>& vertices,
                               std::vector<uint32_t>& indices);
};

} // namespace eng::scene
