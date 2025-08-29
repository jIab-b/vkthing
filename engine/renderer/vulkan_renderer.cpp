#include "vulkan_renderer.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include "../scene/gltf_loader.h"

using namespace eng::renderer;

static bool hasExt(const char* name) {
    uint32_t count = 0; vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count); vkEnumerateInstanceExtensionProperties(nullptr, &count, exts.data());
    for (auto& e: exts) if (std::strcmp(e.extensionName, name) == 0) return true;
    return false;
}

static bool hasLayer(const char* name) {
    uint32_t count = 0; vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count); vkEnumerateInstanceLayerProperties(&count, layers.data());
    for (auto& l : layers) if (std::strcmp(l.layerName, name) == 0) return true;
    return false;
}

bool VulkanRenderer::createInstance() {
    uint32_t extCount = 0; const char** glfwExts = glfwGetRequiredInstanceExtensions(&extCount);
    std::vector<const char*> exts(glfwExts, glfwExts + extCount);
    if (hasExt(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
        exts.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    if (hasExt(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
        exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    std::vector<const char*> layers;
    if (hasLayer("VK_LAYER_KHRONOS_validation")) layers.push_back("VK_LAYER_KHRONOS_validation");

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "vkthing";
    app.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = (uint32_t)exts.size();
    ci.ppEnabledExtensionNames = exts.data();
    ci.flags = hasExt(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) ?
               VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR : 0;
    ci.enabledLayerCount = (uint32_t)layers.size();
    ci.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();
    if (vkCreateInstance(&ci, nullptr, &instance_) != VK_SUCCESS) return false;
    setupDebug();
    return true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL dbgCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*) {
    const char* sev = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "ERROR" :
                      (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "WARN" :
                      (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) ? "INFO" : "VERBOSE";
    fprintf(stderr, "[Vulkan][%s] %s\n", sev, data->pMessage);
    return VK_FALSE;
}

void VulkanRenderer::setupDebug() {
    auto create = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT");
    if (!create) return;
    VkDebugUtilsMessengerCreateInfoEXT ci{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = dbgCallback;
    create(instance_, &ci, nullptr, &debugMessenger_);
}

bool VulkanRenderer::createSurface() {
    return glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) == VK_SUCCESS;
}

bool VulkanRenderer::pickPhysicalDevice() {
    uint32_t count = 0; vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) return false;
    std::vector<VkPhysicalDevice> gpus(count); vkEnumeratePhysicalDevices(instance_, &count, gpus.data());
    for (auto gpu: gpus) {
        // Find queue that supports graphics and present
        uint32_t qCount = 0; vkGetPhysicalDeviceQueueFamilyProperties(gpu, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qprops(qCount); vkGetPhysicalDeviceQueueFamilyProperties(gpu, &qCount, qprops.data());
        for (uint32_t i=0;i<qCount;++i) {
            VkBool32 present = VK_FALSE; vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface_, &present);
            if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
                physical_ = gpu; graphicsQueueFamily_ = i; return true;
            }
        }
    }
    return false;
}

bool VulkanRenderer::createDevice() {
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = graphicsQueueFamily_;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;

    const char* devExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = devExts;
    if (vkCreateDevice(physical_, &dci, nullptr, &device_) != VK_SUCCESS) return false;
    vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);
    presentQueue_ = graphicsQueue_;
    return true;
}

bool VulkanRenderer::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps{}; vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_, surface_, &caps);
    uint32_t fmtCount=0; vkGetPhysicalDeviceSurfaceFormatsKHR(physical_, surface_, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount); vkGetPhysicalDeviceSurfaceFormatsKHR(physical_, surface_, &fmtCount, formats.data());
    VkSurfaceFormatKHR chosen = formats[0];
    for (auto& f: formats) {
        if ((f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM) && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { chosen = f; break; }
    }
    swapFormat_ = chosen.format;

    int fbw=0, fbh=0; glfwGetFramebufferSize(window_, &fbw, &fbh);
    VkExtent2D extent{};
    if (caps.currentExtent.width != UINT32_MAX) extent = caps.currentExtent; else { extent = { (uint32_t)fbw, (uint32_t)fbh }; }
    swapExtent_ = extent;

    uint32_t imageCount = caps.minImageCount + 1; if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;
    VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface = surface_;
    sci.minImageCount = imageCount;
    sci.imageFormat = swapFormat_;
    sci.imageColorSpace = chosen.colorSpace;
    sci.imageExtent = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(device_, &sci, nullptr, &swapchain_) != VK_SUCCESS) return false;
    uint32_t count = 0; vkGetSwapchainImagesKHR(device_, swapchain_, &count, nullptr);
    swapImages_.resize(count); vkGetSwapchainImagesKHR(device_, swapchain_, &count, swapImages_.data());
    return true;
}

bool VulkanRenderer::createImageViews() {
    swapViews_.resize(swapImages_.size());
    for (size_t i=0;i<swapImages_.size();++i) {
        VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ci.image = swapImages_[i];
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = swapFormat_;
        ci.components = {VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY};
        ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.subresourceRange.levelCount = 1;
        ci.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &ci, nullptr, &swapViews_[i]) != VK_SUCCESS) return false;
    }
    return true;
}

bool VulkanRenderer::createRenderPass() {
    VkAttachmentDescription color{};
    color.format = swapFormat_;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    // Depth attachment
    depthFormat_ = findDepthFormat();
    VkAttachmentDescription depth{};
    depth.format = depthFormat_;
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &colorRef;
    sub.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[2] = { color, depth };
    VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpci.attachmentCount = 2;
    rpci.pAttachments = attachments;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &sub;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dep;
    return vkCreateRenderPass(device_, &rpci, nullptr, &renderPass_) == VK_SUCCESS;
}

bool VulkanRenderer::createFramebuffers() {
    framebuffers_.resize(swapViews_.size());
    for (size_t i=0;i<swapViews_.size();++i) {
        VkImageView views[] = { swapViews_[i], depthView_ };
        VkFramebufferCreateInfo fci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = renderPass_;
        fci.attachmentCount = 2;
        fci.pAttachments = views;
        fci.width = swapExtent_.width;
        fci.height = swapExtent_.height;
        fci.layers = 1;
        if (vkCreateFramebuffer(device_, &fci, nullptr, &framebuffers_[i]) != VK_SUCCESS) return false;
    }
    return true;
}

bool VulkanRenderer::createCommands() {
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.queueFamilyIndex = graphicsQueueFamily_;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(device_, &pci, nullptr, &cmdPool_) != VK_SUCCESS) return false;
    cmdBufs_.resize(framebuffers_.size());
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = cmdPool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = (uint32_t)cmdBufs_.size();
    if (vkAllocateCommandBuffers(device_, &ai, cmdBufs_.data()) != VK_SUCCESS) return false;
    return true;
}

bool VulkanRenderer::createSync() {
    semImageAvail_.resize(kMaxFrames);
    semRenderFinish_.resize(kMaxFrames);
    inFlight_.resize(kMaxFrames);
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (int i=0;i<kMaxFrames;++i) {
        if (vkCreateSemaphore(device_, &sci, nullptr, &semImageAvail_[i]) != VK_SUCCESS) return false;
        if (vkCreateSemaphore(device_, &sci, nullptr, &semRenderFinish_[i]) != VK_SUCCESS) return false;
        if (vkCreateFence(device_, &fci, nullptr, &inFlight_[i]) != VK_SUCCESS) return false;
    }
    return true;
}

bool VulkanRenderer::initialize(GLFWwindow* window) {
    window_ = window;
    printf("VulkanRenderer: Creating Vulkan instance...\n");
    if (!createInstance()) { printf("VulkanRenderer: FAILED - createInstance()\n"); return false; }
    printf("VulkanRenderer: Instance created successfully\n");

    printf("VulkanRenderer: Creating surface...\n");
    if (!createSurface()) { printf("VulkanRenderer: FAILED - createSurface()\n"); return false; }
    printf("VulkanRenderer: Surface created successfully\n");

    printf("VulkanRenderer: Picking physical device...\n");
    if (!pickPhysicalDevice()) { printf("VulkanRenderer: FAILED - pickPhysicalDevice()\n"); return false; }
    printf("VulkanRenderer: Physical device selected\n");

    printf("VulkanRenderer: Creating logical device...\n");
    if (!createDevice()) { printf("VulkanRenderer: FAILED - createDevice()\n"); return false; }
    printf("VulkanRenderer: Logical device created\n");

    printf("VulkanRenderer: Creating swapchain...\n");
    if (!createSwapchain()) { printf("VulkanRenderer: FAILED - createSwapchain()\n"); return false; }
    printf("VulkanRenderer: Swapchain created\n");

    printf("VulkanRenderer: Creating image views...\n");
    if (!createImageViews()) { printf("VulkanRenderer: FAILED - createImageViews()\n"); return false; }
    printf("VulkanRenderer: Image views created\n");

    printf("VulkanRenderer: Creating render pass...\n");
    if (!createRenderPass()) { printf("VulkanRenderer: FAILED - createRenderPass()\n"); return false; }
    printf("VulkanRenderer: Render pass created\n");

    printf("VulkanRenderer: Creating depth resources...\n");
    if (!createDepthResources()) { printf("VulkanRenderer: FAILED - createDepthResources()\n"); return false; }
    printf("VulkanRenderer: Depth resources created\n");

    printf("VulkanRenderer: Creating framebuffers...\n");
    if (!createFramebuffers()) { printf("VulkanRenderer: FAILED - createFramebuffers()\n"); return false; }
    printf("VulkanRenderer: Framebuffers created\n");

    printf("VulkanRenderer: Creating command buffers...\n");
    if (!createCommands()) { printf("VulkanRenderer: FAILED - createCommands()\n"); return false; }
    printf("VulkanRenderer: Command buffers created\n");

    printf("VulkanRenderer: Creating synchronization objects...\n");
    if (!createSync()) { printf("VulkanRenderer: FAILED - createSync()\n"); return false; }
    printf("VulkanRenderer: Synchronization objects created\n");

    printf("VulkanRenderer: Creating mesh pipeline...\n");
    // Shaders directory relative to working dir
    if (!createMeshPipeline("shaders")) { printf("VulkanRenderer: FAILED - createMeshPipeline()\n"); return false; }
    printf("VulkanRenderer: Mesh pipeline created successfully\n");

    return true;
}

void VulkanRenderer::cleanupSwapchain() {
    for (auto fb: framebuffers_) vkDestroyFramebuffer(device_, fb, nullptr);
    framebuffers_.clear();
    if (renderPass_) { vkDestroyRenderPass(device_, renderPass_, nullptr); renderPass_ = VK_NULL_HANDLE; }
    if (depthView_ || depthImage_) { destroyDepthResources(); }
    for (auto v: swapViews_) vkDestroyImageView(device_, v, nullptr);
    swapViews_.clear();
    if (swapchain_) { vkDestroySwapchainKHR(device_, swapchain_, nullptr); swapchain_ = VK_NULL_HANDLE; }
}

bool VulkanRenderer::recreateSwapchain() {
    vkDeviceWaitIdle(device_);
    cleanupSwapchain();
    if (!createSwapchain()) return false;
    if (!createImageViews()) return false;
    if (!createRenderPass()) return false;
    if (!createDepthResources()) return false;
    if (!createFramebuffers()) return false;
    // command buffers remain valid since pool persists, but their recorded content depends on render pass/framebuffers; we'll record each frame.
    return true;
}

bool VulkanRenderer::drawFrame(float r, float g, float b) {
    vkWaitForFences(device_, 1, &inFlight_[curFrame_], VK_TRUE, UINT64_MAX);
    vkResetFences(device_, 1, &inFlight_[curFrame_]);

    uint32_t imageIndex = 0;
    VkResult acq = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, semImageAvail_[curFrame_], VK_NULL_HANDLE, &imageIndex);
    if (acq == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapchain(); return true; }
    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) return false;

    VkCommandBuffer cmd = cmdBufs_[imageIndex];
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmd, &bi);

    VkClearValue clears[2]{}; clears[0].color = { r, g, b, 1.0f }; clears[1].depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo rpbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpbi.renderPass = renderPass_;
    rpbi.framebuffer = framebuffers_[imageIndex];
    rpbi.renderArea.offset = {0,0}; rpbi.renderArea.extent = swapExtent_;
    rpbi.clearValueCount = 2; rpbi.pClearValues = clears;
    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    // viewport/scissor
    VkViewport vp{0.0f, 0.0f, (float)swapExtent_.width, (float)swapExtent_.height, 0.0f, 1.0f};
    VkRect2D sc{{0,0}, swapExtent_};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);

    // Render GLTF meshes
    renderMeshes(cmd);
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1; si.pWaitSemaphores = &semImageAvail_[curFrame_]; si.pWaitDstStageMask = &waitStage;
    si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
    si.signalSemaphoreCount = 1; si.pSignalSemaphores = &semRenderFinish_[curFrame_];
    if (vkQueueSubmit(graphicsQueue_, 1, &si, inFlight_[curFrame_]) != VK_SUCCESS) return false;

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = &semRenderFinish_[curFrame_];
    pi.swapchainCount = 1; pi.pSwapchains = &swapchain_;
    pi.pImageIndices = &imageIndex;
    VkResult pres = vkQueuePresentKHR(presentQueue_, &pi);
    if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) { recreateSwapchain(); }
    else if (pres != VK_SUCCESS) return false;

    curFrame_ = (curFrame_ + 1) % kMaxFrames;
    return true;
}

void VulkanRenderer::waitIdle() { if (device_) vkDeviceWaitIdle(device_); }

void VulkanRenderer::shutdown() {
    if (!device_) return;
    vkDeviceWaitIdle(device_);
    for (auto f: inFlight_) vkDestroyFence(device_, f, nullptr);
    for (auto s: semImageAvail_) vkDestroySemaphore(device_, s, nullptr);
    for (auto s: semRenderFinish_) vkDestroySemaphore(device_, s, nullptr);
    inFlight_.clear(); semImageAvail_.clear(); semRenderFinish_.clear();
    if (cmdPool_) { vkDestroyCommandPool(device_, cmdPool_, nullptr); cmdPool_ = VK_NULL_HANDLE; }
    cleanupSwapchain();
    if (meshPipelineLayout_) { vkDestroyPipelineLayout(device_, meshPipelineLayout_, nullptr); meshPipelineLayout_ = VK_NULL_HANDLE; }
    if (meshPipeline_) { vkDestroyPipeline(device_, meshPipeline_, nullptr); meshPipeline_ = VK_NULL_HANDLE; }
    if (meshVbo_) { vkDestroyBuffer(device_, meshVbo_, nullptr); meshVbo_ = VK_NULL_HANDLE; }
    if (meshVboMem_) { vkFreeMemory(device_, meshVboMem_, nullptr); meshVboMem_ = VK_NULL_HANDLE; }
    if (meshIbo_) { vkDestroyBuffer(device_, meshIbo_, nullptr); meshIbo_ = VK_NULL_HANDLE; }
    if (meshIboMem_) { vkFreeMemory(device_, meshIboMem_, nullptr); meshIboMem_ = VK_NULL_HANDLE; }
    if (device_) { vkDestroyDevice(device_, nullptr); device_ = VK_NULL_HANDLE; }
    if (surface_) { vkDestroySurfaceKHR(instance_, surface_, nullptr); surface_ = VK_NULL_HANDLE; }
    if (debugMessenger_) {
        auto destroy = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT");
        if (destroy) destroy(instance_, debugMessenger_, nullptr);
        debugMessenger_ = VK_NULL_HANDLE;
    }
    if (instance_) { vkDestroyInstance(instance_, nullptr); instance_ = VK_NULL_HANDLE; }
}

void VulkanRenderer::setVP(const float* vp16) { std::memcpy(vp_, vp16, sizeof(vp_)); }

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps{}; vkGetPhysicalDeviceMemoryProperties(physical_, &memProps);
    for (uint32_t i=0;i<memProps.memoryTypeCount;++i) if ((typeFilter & (1u<<i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) return i;
    throw std::runtime_error("No suitable memory type");
}

static std::vector<char> readFile(const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return {};
    std::fseek(f, 0, SEEK_END); long sz = std::ftell(f); std::rewind(f);
    std::vector<char> data(sz); std::fread(data.data(), 1, sz, f); std::fclose(f); return data;
}







bool VulkanRenderer::createMeshPipeline(const char* shaderDir) {
    printf("MeshPipeline: Loading shaders from directory: %s\n", shaderDir);

    // Load mesh shaders
    std::string vsPath = std::string(shaderDir) + "/mesh.vert.spv";
    std::string fsPath = std::string(shaderDir) + "/mesh.frag.spv";

    printf("MeshPipeline: Loading vertex shader: %s\n", vsPath.c_str());
    auto vsCode = readFile(vsPath.c_str());
    printf("MeshPipeline: Loading fragment shader: %s\n", fsPath.c_str());
    auto fsCode = readFile(fsPath.c_str());

    if (vsCode.empty()) {
        printf("MeshPipeline: FAILED - Vertex shader file is empty or not found: %s\n", vsPath.c_str());
        return false;
    }
    if (fsCode.empty()) {
        printf("MeshPipeline: FAILED - Fragment shader file is empty or not found: %s\n", fsPath.c_str());
        return false;
    }

    printf("MeshPipeline: Shaders loaded successfully (VS: %zu bytes, FS: %zu bytes)\n", vsCode.size(), fsCode.size());

    VkShaderModule vsMod, fsMod;
    VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};

    printf("MeshPipeline: Creating vertex shader module...\n");
    smci.codeSize = vsCode.size(); smci.pCode = reinterpret_cast<const uint32_t*>(vsCode.data());
    if (vkCreateShaderModule(device_, &smci, nullptr, &vsMod) != VK_SUCCESS) {
        printf("MeshPipeline: FAILED - Could not create vertex shader module\n");
        return false;
    }

    printf("MeshPipeline: Creating fragment shader module...\n");
    smci.codeSize = fsCode.size(); smci.pCode = reinterpret_cast<const uint32_t*>(fsCode.data());
    if (vkCreateShaderModule(device_, &smci, nullptr, &fsMod) != VK_SUCCESS) {
        printf("MeshPipeline: FAILED - Could not create fragment shader module\n");
        vkDestroyShaderModule(device_, vsMod, nullptr);
        return false;
    }

    printf("MeshPipeline: Shader modules created successfully\n");

    VkPipelineShaderStageCreateInfo sstages[2]{};
    sstages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    sstages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; sstages[0].module = vsMod; sstages[0].pName = "main";
    sstages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    sstages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; sstages[1].module = fsMod; sstages[1].pName = "main";

    VkVertexInputBindingDescription vibd{};
    vibd.binding = 0; vibd.stride = sizeof(eng::scene::MeshVertex); vibd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription viads[3]{};
    viads[0].location = 0; viads[0].binding = 0; viads[0].format = VK_FORMAT_R32G32B32_SFLOAT; viads[0].offset = offsetof(eng::scene::MeshVertex, position);
    viads[1].location = 1; viads[1].binding = 0; viads[1].format = VK_FORMAT_R32G32B32_SFLOAT; viads[1].offset = offsetof(eng::scene::MeshVertex, normal);
    viads[2].location = 2; viads[2].binding = 0; viads[2].format = VK_FORMAT_R32G32_SFLOAT; viads[2].offset = offsetof(eng::scene::MeshVertex, texCoord);

    VkPipelineVertexInputStateCreateInfo vis{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vis.vertexBindingDescriptionCount = 1; vis.pVertexBindingDescriptions = &vibd;
    vis.vertexAttributeDescriptionCount = 3; vis.pVertexAttributeDescriptions = viads;

    VkPipelineInputAssemblyStateCreateInfo ias{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ias.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ias.primitiveRestartEnable = VK_FALSE;

    VkViewport vp{0,0,(float)swapExtent_.width,(float)swapExtent_.height,0.0f,1.0f};
    VkRect2D sc{{0,0}, swapExtent_};
    VkPipelineViewportStateCreateInfo vps{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vps.viewportCount = 1; vps.pViewports = &vp; vps.scissorCount = 1; vps.pScissors = &sc;

    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_BACK_BIT; rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cba{}; cba.colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; cb.attachmentCount = 1; cb.pAttachments = &cba;

    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_TRUE; ds.depthWriteEnable = VK_TRUE; ds.depthCompareOp = VK_COMPARE_OP_LESS;

    VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO}; dyn.dynamicStateCount = 2; dyn.pDynamicStates = dynStates;

    // Create mesh pipeline layout
    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(float) * 16 + sizeof(float) * 4 * 3;  // VP matrix + light data
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(device_, &plci, nullptr, &meshPipelineLayout_) != VK_SUCCESS) {
        vkDestroyShaderModule(device_, vsMod, nullptr);
        vkDestroyShaderModule(device_, fsMod, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pci.stageCount = 2; pci.pStages = sstages;
    pci.pVertexInputState = &vis;
    pci.pInputAssemblyState = &ias;
    pci.pViewportState = &vps;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState = &ms;
    pci.pColorBlendState = &cb;
    pci.pDepthStencilState = &ds;
    pci.pDynamicState = &dyn;
    pci.layout = meshPipelineLayout_;
    pci.renderPass = renderPass_;
    pci.subpass = 0;
    bool ok = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pci, nullptr, &meshPipeline_) == VK_SUCCESS;
    vkDestroyShaderModule(device_, vsMod, nullptr); vkDestroyShaderModule(device_, fsMod, nullptr);
    return ok;
}

bool VulkanRenderer::createMeshGeometry(const std::vector<eng::scene::Mesh>& meshes) {
    if (meshes.empty()) return true;

    // Calculate total vertices and indices
    size_t totalVertices = 0;
    size_t totalIndices = 0;
    meshTransforms_.clear();

    for (const auto& mesh : meshes) {
        totalVertices += mesh.vertices.size();
        totalIndices += mesh.indices.size();
        meshTransforms_.push_back(mesh.transform);
    }

    meshVertexCount_ = static_cast<uint32_t>(totalVertices);
    meshIndexCount_ = static_cast<uint32_t>(totalIndices);

    if (totalVertices == 0) return true;

    // Create vertex buffer
    VkDeviceSize vertexSize = totalVertices * sizeof(eng::scene::MeshVertex);
    VkBufferCreateInfo vbci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    vbci.size = vertexSize;
    vbci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vbci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &vbci, nullptr, &meshVbo_) != VK_SUCCESS) return false;
    VkMemoryRequirements vmr{};
    vkGetBufferMemoryRequirements(device_, meshVbo_, &vmr);
    VkMemoryAllocateInfo vmai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    vmai.allocationSize = vmr.size;
    vmai.memoryTypeIndex = findMemoryType(vmr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(device_, &vmai, nullptr, &meshVboMem_) != VK_SUCCESS) return false;
    vkBindBufferMemory(device_, meshVbo_, meshVboMem_, 0);

    // Create index buffer if we have indices
    if (totalIndices > 0) {
        VkDeviceSize indexSize = totalIndices * sizeof(uint32_t);
        VkBufferCreateInfo ibci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        ibci.size = indexSize;
        ibci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        ibci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device_, &ibci, nullptr, &meshIbo_) != VK_SUCCESS) return false;
        VkMemoryRequirements imr{};
        vkGetBufferMemoryRequirements(device_, meshIbo_, &imr);
        VkMemoryAllocateInfo imai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        imai.allocationSize = imr.size;
        imai.memoryTypeIndex = findMemoryType(imr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkAllocateMemory(device_, &imai, nullptr, &meshIboMem_) != VK_SUCCESS) return false;
        vkBindBufferMemory(device_, meshIbo_, meshIboMem_, 0);

        // Upload index data
        void* indexPtr = nullptr;
        vkMapMemory(device_, meshIboMem_, 0, indexSize, 0, &indexPtr);

        size_t indexOffset = 0;
        uint32_t vertexOffset = 0;
        for (size_t i = 0; i < meshes.size(); ++i) {
            const auto& mesh = meshes[i];
            uint32_t* dstIndices = static_cast<uint32_t*>(indexPtr) + indexOffset;

            for (size_t j = 0; j < mesh.indices.size(); ++j) {
                dstIndices[j] = mesh.indices[j] + vertexOffset;
            }

            indexOffset += mesh.indices.size();
            vertexOffset += static_cast<uint32_t>(mesh.vertices.size());
        }

        vkUnmapMemory(device_, meshIboMem_);
    }

    // Upload vertex data
    void* vertexPtr = nullptr;
    vkMapMemory(device_, meshVboMem_, 0, vertexSize, 0, &vertexPtr);

    eng::scene::MeshVertex* dstVertices = static_cast<eng::scene::MeshVertex*>(vertexPtr);
    size_t vertexOffset = 0;

    for (const auto& mesh : meshes) {
        std::memcpy(dstVertices + vertexOffset, mesh.vertices.data(),
                   mesh.vertices.size() * sizeof(eng::scene::MeshVertex));
        vertexOffset += mesh.vertices.size();
    }

    vkUnmapMemory(device_, meshVboMem_);
    return true;
}

void VulkanRenderer::renderMeshes(VkCommandBuffer cmd) {
    if (!meshPipeline_ || !meshVbo_ || meshVertexCount_ == 0) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline_);

    struct Push { float vp[16]; float pc0[4]; float lightDir[4]; float lightColor[4]; } push{};
    std::memcpy(push.vp, vp_, sizeof(vp_));
    push.pc0[0] = 1.0f;
    push.lightDir[0] = lightDir_[0]; push.lightDir[1] = lightDir_[1]; push.lightDir[2] = lightDir_[2];
    push.lightColor[0] = lightColor_[0]; push.lightColor[1] = lightColor_[1];
    push.lightColor[2] = lightColor_[2]; push.lightColor[3] = lightIntensity_;

    vkCmdPushConstants(cmd, meshPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Push), &push);

    VkDeviceSize vboOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &meshVbo_, &vboOffset);

    if (meshIbo_ && meshIndexCount_ > 0) {
        vkCmdBindIndexBuffer(cmd, meshIbo_, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, meshIndexCount_, 1, 0, 0, 0);
    } else {
        vkCmdDraw(cmd, meshVertexCount_, 1, 0, 0);
    }
}

void VulkanRenderer::loadGltfMeshes(const std::vector<eng::scene::Mesh>& meshes) {
    // Try to create mesh pipeline if it doesn't exist
    if (!meshPipeline_) {
        createMeshPipeline("shaders");
    }

    if (!meshPipeline_) {
        throw std::runtime_error("Failed to create mesh pipeline - mesh shaders not found");
    }

    if (!createMeshGeometry(meshes)) {
        throw std::runtime_error("Failed to create mesh geometry");
    }
}

VkFormat VulkanRenderer::findDepthFormat() {
    VkFormat candidates[] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM };
    for (VkFormat f : candidates) {
        VkFormatProperties props{}; vkGetPhysicalDeviceFormatProperties(physical_, f, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) return f;
    }
    return VK_FORMAT_D32_SFLOAT;
}

bool VulkanRenderer::createDepthResources() {
    VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = depthFormat_;
    ici.extent = { swapExtent_.width, swapExtent_.height, 1 };
    ici.mipLevels = 1; ici.arrayLayers = 1; ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL; ici.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (vkCreateImage(device_, &ici, nullptr, &depthImage_) != VK_SUCCESS) return false;
    VkMemoryRequirements mr{}; vkGetImageMemoryRequirements(device_, depthImage_, &mr);
    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size; mai.memoryTypeIndex = findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device_, &mai, nullptr, &depthMem_) != VK_SUCCESS) return false;
    vkBindImageMemory(device_, depthImage_, depthMem_, 0);
    VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vci.image = depthImage_; vci.viewType = VK_IMAGE_VIEW_TYPE_2D; vci.format = depthFormat_;
    vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; vci.subresourceRange.levelCount = 1; vci.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device_, &vci, nullptr, &depthView_) != VK_SUCCESS) return false;
    return true;
}

void VulkanRenderer::destroyDepthResources() {
    if (depthView_) { vkDestroyImageView(device_, depthView_, nullptr); depthView_ = VK_NULL_HANDLE; }
    if (depthImage_) { vkDestroyImage(device_, depthImage_, nullptr); depthImage_ = VK_NULL_HANDLE; }
    if (depthMem_) { vkFreeMemory(device_, depthMem_, nullptr); depthMem_ = VK_NULL_HANDLE; }
}
