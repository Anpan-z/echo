#pragma once

#include "swap_chain_manager.hpp"
#include <vulkan/vulkan.h>
#include <vector>

class RenderTarget {
public:
    void init(VulkanContext& vulkanContext, SwapChainManager& swapChainManager, VkRenderPass renderPass);
    void cleanup();

    void recreateRenderTarget(VulkanContext& vulkanContext, SwapChainManager& swapChainManager, VkRenderPass renderPass){
        cleanup();
        init(vulkanContext, swapChainManager, renderPass);
    }

    const std::vector<VkFramebuffer>& getFramebuffers() const { return framebuffers; }
    VkImageView getDepthImageView() const { return depthImageView; }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    SwapChainManager* swapChainManager = nullptr;

    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> framebuffers;
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT; // MSAA采样数

    //MASS
    VkImage msaaColorImage;
    VkDeviceMemory msaaColorImageMemory;
    VkImageView msaaColorImageView;

    void createDepthResources();
    void createFramebuffers(const std::vector<VkImageView>& swapChainImageViews, VkRenderPass renderPass);
    void createMsaaColorResources();
    VkFormat findDepthFormat();
};