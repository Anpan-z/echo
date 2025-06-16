#pragma once

#include "swap_chain_manager.hpp"
#include <vector>
#include <vulkan/vulkan.h>

class RenderTarget
{
  public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice, SwapChainManager& swapChainManager,
              VkRenderPass renderPass, VkRenderPass offScreenRenderPass);
    void cleanupOffScreenTarget();
    void cleanupPresentTarget();
    void cleanup();

    void recreateRenderTarget(VkExtent2D imageExtent)
    {
        vkDeviceWaitIdle(device);
        recreateOffScreenTarget(imageExtent);
        recreatePresentTarget();
    }

    void recreateOffScreenTarget(VkExtent2D imageExtent)
    {
        vkDeviceWaitIdle(device);
        cleanupOffScreenTarget();
        this->offScreenExtent = imageExtent;
        createOffScreenTarget(imageExtent, offScreenRenderPass);
    }

    void recreatePresentTarget()
    {
        vkDeviceWaitIdle(device);
        cleanupPresentTarget();
        createPresentTarget(swapChainManager->getSwapChainImageViews(), presentRenderPass);
    }
    void createOffScreenTarget(VkExtent2D imageExtent, VkRenderPass offScreenRenderPass)
    {
        createDepthResources(imageExtent);
        createOffScreenResources(imageExtent);
        createOffScreenFramebuffers(offScreenRenderPass, imageExtent);
        createOffScreenSampler();
    }
    void createPresentTarget(const std::vector<VkImageView>& swapChainImageViews, VkRenderPass renderPass)
    {
        createFramebuffers(swapChainImageViews, renderPass);
    }
    const VkExtent2D& getOffScreenExtent() const
    {
        return offScreenExtent;
    }
    const std::vector<VkFramebuffer>& getFramebuffers() const
    {
        return framebuffers;
    }
    const std::vector<VkFramebuffer>& getOffScreenFramebuffers() const
    {
        return offScreenFramebuffers;
    }
    const std::vector<VkImageView>& getDepthImageView() const
    {
        return depthImageViews;
    }
    const std::vector<VkImageView>& getOffScreenImageView() const
    {
        return offScreenImageViews;
    }
    const std::vector<VkImage>& getOffScreenImages() const
    {
        return offScreenImages;
    }
    VkSampler getOffScreenSampler() const
    {
        return offScreenSampler;
    }

  private:
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    SwapChainManager* swapChainManager = nullptr;

    VkExtent2D offScreenExtent;

    VkRenderPass presentRenderPass = VK_NULL_HANDLE;
    VkRenderPass offScreenRenderPass = VK_NULL_HANDLE;

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
    void createOffScreenFramebuffers(VkRenderPass renderPass, VkExtent2D imageExtent);

    void createOffScreenSampler();
    VkFormat findDepthFormat();
};