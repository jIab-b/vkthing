#pragma once
#include <vector>
#include <glm/glm.hpp>

namespace eng::terrain {
    struct Settings {
        int chunkPoints = 64;    // N x N points per chunk
        float spacing = 1.0f;    // world units between points
        int radiusChunks = 4;    // generate a (2r+1)^2 grid of chunks
        float heightScale = 60.0f;
        float frequency = 0.005f;
        int octaves = 5;
    };

    struct Vertex { glm::vec3 pos; glm::vec3 normal; float height; };

    // Simple hash-based value noise FBM for demo
    float noise2D(float x, float y);
    float fbm(float x, float y, int octaves);

    std::vector<Vertex> generate(const Settings& s);
}
