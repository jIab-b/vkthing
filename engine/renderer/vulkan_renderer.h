#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <cstring>
#include <glm/glm.hpp>
struct GLFWwindow;

namespace eng::scene { struct Mesh; }

namespace eng::renderer {
    class VulkanRenderer {
    public:
        bool initialize(GLFWwindow* window);
        void shutdown();
        bool drawFrame(float clearR=0.02f, float clearG=0.02f, float clearB=0.08f);
        void waitIdle();
        void setVP(const float* vp16);
        void setPointSize(float sz) { pointSize_ = sz; }

        // GLTF mesh support
        void loadGltfMeshes(const std::vector<eng::scene::Mesh>& meshes);
    private:
        GLFWwindow* window_ = nullptr;
        VkInstance instance_{};
        VkDebugUtilsMessengerEXT debugMessenger_{};
        VkSurfaceKHR surface_{};
        VkPhysicalDevice physical_{};
        VkDevice device_{};
        uint32_t graphicsQueueFamily_ = 0;
        VkQueue graphicsQueue_{};
        VkQueue presentQueue_{};
        VkSwapchainKHR swapchain_{};
        VkFormat swapFormat_{};
        VkExtent2D swapExtent_{};
        std::vector<VkImage> swapImages_;
        std::vector<VkImageView> swapViews_;
        VkRenderPass renderPass_{};
        std::vector<VkFramebuffer> framebuffers_;
        VkCommandPool cmdPool_{};
        std::vector<VkCommandBuffer> cmdBufs_;
        static constexpr int kMaxFrames = 2;
        uint32_t curFrame_ = 0;
        std::vector<VkSemaphore> semImageAvail_;
        std::vector<VkSemaphore> semRenderFinish_;
        std::vector<VkFence> inFlight_;
        float vp_[16] = {0}; float pointSize_ = 3.0f;
        // Mesh pipeline + geometry
        VkPipelineLayout meshPipelineLayout_{};
        VkPipeline meshPipeline_{};
        VkBuffer meshVbo_{}; VkDeviceMemory meshVboMem_{};
        VkBuffer meshIbo_{}; VkDeviceMemory meshIboMem_{};
        uint32_t meshVertexCount_ = 0;
        uint32_t meshIndexCount_ = 0;
        std::vector<glm::mat4> meshTransforms_;
        // Depth
        VkImage depthImage_{}; VkDeviceMemory depthMem_{}; VkImageView depthView_{}; VkFormat depthFormat_{};
        // Light
        float lightDir_[3] = {-0.5f,-1.0f,-0.25f};
        float lightColor_[3] = {1.0f, 0.98f, 0.9f};
        float lightIntensity_ = 2.0f;

        bool createInstance();
        void setupDebug();
        bool createSurface();
        bool pickPhysicalDevice();
        bool createDevice();
        bool createSwapchain();
        bool createImageViews();
        bool createRenderPass();
        bool createFramebuffers();
        bool createCommands();
        bool createSync();
        void cleanupSwapchain();
        bool recreateSwapchain();

        // helpers
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props);
        VkFormat findDepthFormat();
        bool createDepthResources();
        void destroyDepthResources();

        // Mesh pipeline methods
        bool createMeshPipeline(const char* shaderDir);
        bool createMeshGeometry(const std::vector<eng::scene::Mesh>& meshes);
        void renderMeshes(VkCommandBuffer cmd);
    public:
        void setLight(const float dir[3], const float color[3], float intensity) {
            std::memcpy(lightDir_, dir, sizeof(lightDir_));
            std::memcpy(lightColor_, color, sizeof(lightColor_));
            lightIntensity_ = intensity;
        }
    };
}
