#pragma once

#include "swap_chain_manager.hpp"
#include <vulkan/vulkan.h>
#include <vector>

class RenderTarget {
public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice, SwapChainManager& swapChainManager, VkRenderPass renderPass, VkRenderPass offScreenRenderPass);
    void cleanup();

    void recreateRenderTarget(SwapChainManager& swapChainManager, VkRenderPass renderPass, VkRenderPass offScreenRenderPass) {
        cleanup();
        init(device, physicalDevice, swapChainManager, renderPass, offScreenRenderPass);
    }
    void createOffScreenTarget(VkExtent2D imageExtent, VkRenderPass offScreenRenderPass){
        createDepthResources(imageExtent);
        createOffScreenResources(imageExtent);
        createOffScreenFramebuffers(offScreenRenderPass);
        createOffScreenSampler();
    }
    void createPresentTarget(const std::vector<VkImageView>& swapChainImageViews, VkRenderPass renderPass){
        createFramebuffers(swapChainImageViews, renderPass);
    }
    const std::vector<VkFramebuffer>& getFramebuffers() const { return framebuffers; }
    const std::vector<VkFramebuffer>& getOffScreenFramebuffers() const { return offScreenFramebuffers; }
    const std::vector<VkImageView>& getDepthImageView() const { return depthImageViews; }
    const std::vector<VkImageView>& getOffScreenImageView() const { return offScreenImageViews; }
    const std::vector<VkImage>& getOffScreenImages() const { return offScreenImages; }
    VkSampler getOffScreenSampler() const { return offScreenSampler; }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    SwapChainManager* swapChainManager = nullptr;

    std::vector<VkImage> depthImages;
    std::vector<VkDeviceMemory> depthImageMemories;
    std::vector<VkImageView> depthImageViews;

    std::vector<VkFramebuffer> framebuffers;

    std::vector<VkImage> offScreenImages;
    std::vector<VkDeviceMemory> offScreenImageMemories;
    std::vector<VkImageView> offScreenImageViews;
    std::vector<VkFramebuffer> offScreenFramebuffers;

    VkSampler offScreenSampler = VK_NULL_HANDLE;

    void createDepthResources(VkExtent2D imageExtent);
    void createOffScreenResources(VkExtent2D imageExtent);
    void createFramebuffers(const std::vector<VkImageView>& swapChainImageViews, VkRenderPass renderPass);
    void createOffScreenFramebuffers(VkRenderPass renderPass);

    void createOffScreenSampler();
    VkFormat findDepthFormat();
};