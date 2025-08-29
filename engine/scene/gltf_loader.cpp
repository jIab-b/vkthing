#include "gltf_loader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <stdexcept>

namespace eng::scene {

glm::mat4 GltfLoader::getNodeTransform(const tinygltf::Node& node) {
    glm::mat4 transform(1.0f);

    if (!node.matrix.empty()) {
        // Use matrix if provided
        for (int i = 0; i < 16; ++i) {
            transform[i / 4][i % 4] = static_cast<float>(node.matrix[i]);
        }
    } else {
        // Use TRS (Translation, Rotation, Scale)
        glm::vec3 translation(0.0f);
        glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 scale(1.0f);

        if (!node.translation.empty()) {
            translation = glm::vec3(
                static_cast<float>(node.translation[0]),
                static_cast<float>(node.translation[1]),
                static_cast<float>(node.translation[2])
            );
        }

        if (!node.rotation.empty()) {
            rotation = glm::quat(
                static_cast<float>(node.rotation[3]),  // w
                static_cast<float>(node.rotation[0]),  // x
                static_cast<float>(node.rotation[1]),  // y
                static_cast<float>(node.rotation[2])   // z
            );
        }

        if (!node.scale.empty()) {
            scale = glm::vec3(
                static_cast<float>(node.scale[0]),
                static_cast<float>(node.scale[1]),
                static_cast<float>(node.scale[2])
            );
        }

        transform = glm::translate(glm::mat4(1.0f), translation) *
                   glm::mat4_cast(rotation) *
                   glm::scale(glm::mat4(1.0f), scale);
    }

    return transform;
}

void GltfLoader::extractMeshData(const tinygltf::Model& model,
                                const tinygltf::Primitive& primitive,
                                std::vector<MeshVertex>& vertices,
                                std::vector<uint32_t>& indices) {

    // Get position attribute
    auto posIt = primitive.attributes.find("POSITION");
    if (posIt == primitive.attributes.end()) return;

    const tinygltf::Accessor& posAccessor = model.accessors[posIt->second];
    const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
    const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];
    const float* posData = reinterpret_cast<const float*>(
        &posBuffer.data[posView.byteOffset + posAccessor.byteOffset]);

    // Get normal attribute
    const float* normalData = nullptr;
    auto normalIt = primitive.attributes.find("NORMAL");
    if (normalIt != primitive.attributes.end()) {
        const tinygltf::Accessor& normalAccessor = model.accessors[normalIt->second];
        const tinygltf::BufferView& normalView = model.bufferViews[normalAccessor.bufferView];
        const tinygltf::Buffer& normalBuffer = model.buffers[normalView.buffer];
        normalData = reinterpret_cast<const float*>(
            &normalBuffer.data[normalView.byteOffset + normalAccessor.byteOffset]);
    }

    // Get texcoord attribute
    const float* texCoordData = nullptr;
    auto texIt = primitive.attributes.find("TEXCOORD_0");
    if (texIt != primitive.attributes.end()) {
        const tinygltf::Accessor& texAccessor = model.accessors[texIt->second];
        const tinygltf::BufferView& texView = model.bufferViews[texAccessor.bufferView];
        const tinygltf::Buffer& texBuffer = model.buffers[texView.buffer];
        texCoordData = reinterpret_cast<const float*>(
            &texBuffer.data[texView.byteOffset + texAccessor.byteOffset]);
    }

    // Extract vertices
    vertices.resize(posAccessor.count);
    for (size_t i = 0; i < posAccessor.count; ++i) {
        MeshVertex& vertex = vertices[i];
        vertex.position = glm::vec3(posData[i * 3], posData[i * 3 + 1], posData[i * 3 + 2]);

        if (normalData) {
            vertex.normal = glm::vec3(normalData[i * 3], normalData[i * 3 + 1], normalData[i * 3 + 2]);
        } else {
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f); // Default normal
        }

        if (texCoordData) {
            vertex.texCoord = glm::vec2(texCoordData[i * 2], texCoordData[i * 2 + 1]);
        } else {
            vertex.texCoord = glm::vec2(0.0f, 0.0f);
        }
    }

    // Extract indices
    if (primitive.indices >= 0) {
        const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
        const tinygltf::BufferView& indexView = model.bufferViews[indexAccessor.bufferView];
        const tinygltf::Buffer& indexBuffer = model.buffers[indexView.buffer];

        indices.resize(indexAccessor.count);

        if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            const uint16_t* indexData = reinterpret_cast<const uint16_t*>(
                &indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset]);
            for (size_t i = 0; i < indexAccessor.count; ++i) {
                indices[i] = static_cast<uint32_t>(indexData[i]);
            }
        } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
            const uint32_t* indexData = reinterpret_cast<const uint32_t*>(
                &indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset]);
            std::memcpy(indices.data(), indexData, indexAccessor.count * sizeof(uint32_t));
        }
    }
}

void GltfLoader::processMesh(const tinygltf::Model& model,
                            const tinygltf::Mesh& mesh,
                            const glm::mat4& transform,
                            std::vector<Mesh>& outMeshes) {

    for (const auto& primitive : mesh.primitives) {
        if (primitive.mode != TINYGLTF_MODE_TRIANGLES) continue; // Only triangles for now

        Mesh newMesh;
        newMesh.transform = transform;

        extractMeshData(model, primitive, newMesh.vertices, newMesh.indices);

        if (!newMesh.vertices.empty()) {
            outMeshes.push_back(std::move(newMesh));
        }
    }
}

std::vector<Mesh> GltfLoader::loadScene(const std::string& gltfPath) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;

    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, gltfPath);

    if (!warn.empty()) {
        std::cout << "GLTF Warning: " << warn << std::endl;
    }

    if (!err.empty()) {
        std::cerr << "GLTF Error: " << err << std::endl;
        return {};
    }

    if (!ret) {
        std::cerr << "Failed to load GLTF file: " << gltfPath << std::endl;
        return {};
    }

    std::vector<Mesh> meshes;

    // Process default scene
    if (model.defaultScene >= 0) {
        const tinygltf::Scene& scene = model.scenes[model.defaultScene];

        for (int nodeIndex : scene.nodes) {
            std::vector<std::pair<int, glm::mat4>> nodeStack;
            nodeStack.emplace_back(nodeIndex, glm::mat4(1.0f));

            while (!nodeStack.empty()) {
                auto [currentNodeIndex, currentTransform] = nodeStack.back();
                nodeStack.pop_back();

                const tinygltf::Node& node = model.nodes[currentNodeIndex];
                glm::mat4 nodeTransform = currentTransform * getNodeTransform(node);

                // Process mesh if present
                if (node.mesh >= 0) {
                    const tinygltf::Mesh& mesh = model.meshes[node.mesh];
                    processMesh(model, mesh, nodeTransform, meshes);
                }

                // Add children to stack
                for (int childIndex : node.children) {
                    nodeStack.emplace_back(childIndex, nodeTransform);
                }
            }
        }
    }

    std::cout << "Loaded " << meshes.size() << " meshes from GLTF scene" << std::endl;
    return meshes;
}

} // namespace eng::scene
