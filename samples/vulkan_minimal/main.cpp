#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <cstdlib>

int main() {
    uint32_t count = 0;
    VkResult res = vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    if (res != VK_SUCCESS) {
        std::cerr << "vkEnumerateInstanceExtensionProperties failed: " << res << "\n";
        return EXIT_FAILURE;
    }
    std::vector<VkExtensionProperties> exts(count);
    res = vkEnumerateInstanceExtensionProperties(nullptr, &count, exts.data());
    if (res != VK_SUCCESS) {
        std::cerr << "vkEnumerateInstanceExtensionProperties (2) failed: " << res << "\n";
        return EXIT_FAILURE;
    }
    std::cout << "Vulkan instance extensions available: " << count << "\n";
    for (const auto &e : exts) {
        std::cout << " - " << e.extensionName << " (" << e.specVersion << ")\n";
    }
    return EXIT_SUCCESS;
}

