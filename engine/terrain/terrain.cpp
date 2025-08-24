#include "terrain.h"
#include <cmath>

using namespace eng::terrain;

static inline float fract(float x){ return x - std::floor(x); }
static inline float lerp(float a, float b, float t){ return a + (b - a) * t; }
static inline float smoothstep(float t){ return t * t * (3.0f - 2.0f * t); }

static float hash2(int x, int y) {
    unsigned int h = (unsigned int)(x) * 374761393u + (unsigned int)(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)(h & 0x00FFFFFFu) / (float)0x01000000u; // [0,1)
}

float eng::terrain::noise2D(float x, float y) {
    int xi = (int)std::floor(x);
    int yi = (int)std::floor(y);
    float tx = fract(x);
    float ty = fract(y);
    float a = hash2(xi, yi);
    float b = hash2(xi+1, yi);
    float c = hash2(xi, yi+1);
    float d = hash2(xi+1, yi+1);
    float u = smoothstep(tx);
    float v = smoothstep(ty);
    float ab = lerp(a, b, u);
    float cd = lerp(c, d, u);
    return lerp(ab, cd, v) * 2.0f - 1.0f; // [-1,1]
}

float eng::terrain::fbm(float x, float y, int octaves) {
    float amp = 0.5f, freq = 1.0f, sum = 0.0f;
    for (int i=0;i<octaves;++i) {
        sum += amp * noise2D(x * freq, y * freq);
        freq *= 2.0f; amp *= 0.5f;
    }
    return sum;
}

static inline float height_fn(float x, float z, const Settings& s){
    float h = fbm(x * s.frequency, z * s.frequency, s.octaves);
    h = 1.0f - std::abs(h); // ridge
    h = h * h;
    return h * s.heightScale;
}

std::vector<Vertex> eng::terrain::generate(const Settings& s) {
    std::vector<Vertex> verts;
    int N = s.chunkPoints;
    int R = s.radiusChunks;
    verts.reserve((size_t)(2*R+1)*(2*R+1)*N*N);
    const float e = s.spacing; // derivative step
    for (int cy=-R; cy<=R; ++cy) {
        for (int cx=-R; cx<=R; ++cx) {
            float baseX = cx * (N-1) * s.spacing;
            float baseY = cy * (N-1) * s.spacing;
            for (int j=0;j<N;++j) {
                for (int i=0;i<N;++i) {
                    float x = baseX + i * s.spacing;
                    float z = baseY + j * s.spacing;
                    float y = height_fn(x, z, s);
                    float hx = height_fn(x+e, z, s);
                    float hz = height_fn(x, z+e, s);
                    // Tangents in world space
                    glm::vec3 tx = glm::normalize(glm::vec3(e, hx - y, 0.0f));
                    glm::vec3 tz = glm::normalize(glm::vec3(0.0f, hz - y, e));
                    glm::vec3 n = glm::normalize(glm::cross(tz, tx));
                    verts.push_back({{x, y, z}, n, y});
                }
            }
        }
    }
    return verts;
}
