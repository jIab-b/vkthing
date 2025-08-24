#pragma once
namespace eng::renderer {
    struct RendererConfig { };
    class Renderer {
    public:
        bool initialize(const RendererConfig&) { return true; }
        void shutdown() {}
        void beginFrame() {}
        void endFrame() {}
    };
}

