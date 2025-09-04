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
        enum ExportFlags : uint32_t { ExportColor = 1u<<0, ExportNormal = 1u<<1, ExportMotion = 1u<<2, ExportDepth = 1u<<3 };
        void enableExports(uint32_t flags);
        struct ExportsCPU { void* color; void* normal; void* motion; void* depth; uint32_t width; uint32_t height; size_t colorSize; size_t normalSize; size_t motionSize; size_t depthSize; uint32_t validMask; };
        bool getLatestExports(ExportsCPU& out);

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
        float vp_[16] = {0}; float prevVp_[16] = {0}; float pointSize_ = 3.0f;
        // Mesh pipeline + geometry
        VkPipelineLayout meshPipelineLayout_{};
        VkPipeline meshPipeline_{};
        VkBuffer meshVbo_{}; VkDeviceMemory meshVboMem_{};
        VkBuffer meshIbo_{}; VkDeviceMemory meshIboMem_{};
        uint32_t meshVertexCount_ = 0;
        uint32_t meshIndexCount_ = 0;
        std::vector<glm::mat4> meshTransforms_;
        struct Draw { uint32_t firstIndex; uint32_t indexCount; uint32_t vertexOffset; glm::mat4 model; };
        std::vector<Draw> draws_;
        // Depth
        VkImage depthImage_{}; VkDeviceMemory depthMem_{}; VkImageView depthView_{}; VkFormat depthFormat_{};
        std::vector<VkImage> gbufColorImages_; std::vector<VkDeviceMemory> gbufColorMems_; std::vector<VkImageView> gbufColorViews_; VkFormat gbufColorFormat_{};
        std::vector<VkImage> gbufNormalImages_; std::vector<VkDeviceMemory> gbufNormalMems_; std::vector<VkImageView> gbufNormalViews_; VkFormat gbufNormalFormat_{};
        std::vector<VkImage> gbufMotionImages_; std::vector<VkDeviceMemory> gbufMotionMems_; std::vector<VkImageView> gbufMotionViews_; VkFormat gbufMotionFormat_{};
        VkDescriptorSetLayout descSetLayout_{}; VkDescriptorPool descPool_{}; VkDescriptorSet descSets_[kMaxFrames]{};
        VkBuffer uboBuffers_[kMaxFrames]{}; VkDeviceMemory uboMem_[kMaxFrames]{};
        uint32_t exportFlags_ = 0;
        VkBuffer colorStage_[kMaxFrames]{}; VkDeviceMemory colorStageMem_[kMaxFrames]{}; void* colorMap_[kMaxFrames]{}; size_t colorSizeBytes_ = 0;
        VkBuffer normalStage_[kMaxFrames]{}; VkDeviceMemory normalStageMem_[kMaxFrames]{}; void* normalMap_[kMaxFrames]{}; size_t normalSizeBytes_ = 0;
        VkBuffer motionStage_[kMaxFrames]{}; VkDeviceMemory motionStageMem_[kMaxFrames]{}; void* motionMap_[kMaxFrames]{}; size_t motionSizeBytes_ = 0;
        VkBuffer depthStage_[kMaxFrames]{}; VkDeviceMemory depthStageMem_[kMaxFrames]{}; void* depthMap_[kMaxFrames]{}; size_t depthSizeBytes_ = 0;
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
        bool createGBufferResources();
        void destroyGBufferResources();
        bool createDescriptors();
        void destroyDescriptors();
        bool createExportStaging();
        void destroyExportStaging();

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
