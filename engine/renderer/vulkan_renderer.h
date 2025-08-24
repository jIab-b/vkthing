#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <cstring>
struct GLFWwindow;

namespace eng::renderer {
    class VulkanRenderer {
    public:
        bool initialize(GLFWwindow* window);
        void shutdown();
        bool drawFrame(float clearR=0.02f, float clearG=0.02f, float clearB=0.08f);
        void waitIdle();
        void setVP(const float* vp16);
        void setPointSize(float sz) { pointSize_ = sz; }
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
        // Terrain pipeline + geometry
        VkPipelineLayout pipeLayout_{};
        VkPipeline pipeline_{};
        VkBuffer vbo_{}; VkDeviceMemory vboMem_{}; uint32_t vertexCount_ = 0;
        float vp_[16] = {0}; float pointSize_ = 3.0f;
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
        bool createTerrainPipeline(const char* shaderDir);
        bool createTerrainGeometry();
        VkFormat findDepthFormat();
        bool createDepthResources();
        void destroyDepthResources();
    public:
        void setLight(const float dir[3], const float color[3], float intensity) {
            std::memcpy(lightDir_, dir, sizeof(lightDir_));
            std::memcpy(lightColor_, color, sizeof(lightColor_));
            lightIntensity_ = intensity;
        }
    };
}
