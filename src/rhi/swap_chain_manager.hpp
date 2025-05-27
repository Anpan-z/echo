#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "vulkan_context.hpp"

class SwapChainManager {
public:
    void init(VkDevice device, VulkanContext& vulkanContext, GLFWwindow* window);
    void cleanup();

    void recreateSwapChain();

    VkSwapchainKHR getSwapChain() const { return swapChain; }
    const std::vector<VkImage>& getSwapChainImages() const { return swapChainImages; }
    const std::vector<VkImageView>& getSwapChainImageViews() const { return swapChainImageViews; }
    VkFormat getSwapChainImageFormat() const { return swapChainImageFormat; }
    VkExtent2D getSwapChainExtent() const { return swapChainExtent; }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

private:
    VkDevice device = VK_NULL_HANDLE;
    // VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    // VkSurfaceKHR surface = VK_NULL_HANDLE;

    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    VkFormat swapChainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapChainExtent{};

    VulkanContext* vulkanContext = nullptr;
    GLFWwindow* window = nullptr;

    void createSwapChain();
    void createImageViews();
    void cleanupSwapChain();
};