#include "render_target.hpp"
#include "vulkan_utils.hpp"
#include <array>
#include <stdexcept>

void RenderTarget::init(VkDevice device, VkPhysicalDevice physicalDevice, SwapChainManager& swapChainManager,
                        VkRenderPass offScreenRenderPass, VkRenderPass renderPass)
{
    this->device = device;
    this->physicalDevice = physicalDevice;
    this->swapChainManager = &swapChainManager;
    this->offScreenExtent = swapChainManager.getSwapChainExtent();
    this->offScreenRenderPass = offScreenRenderPass;
    this->presentRenderPass = renderPass;

    createDepthResources(swapChainManager.getSwapChainExtent());
    createFramebuffers(swapChainManager.getSwapChainImageViews(), renderPass);
    createOffScreenResources(swapChainManager.getSwapChainExtent());
    createOffScreenFramebuffers(offScreenRenderPass, swapChainManager.getSwapChainExtent());
    createOffScreenSampler();
}

void RenderTarget::cleanupOffScreenTarget()
{
    vkDeviceWaitIdle(device);
    for (size_t i = 0; i < depthImages.size(); i++)
    {
        vkDestroyImageView(device, depthImageViews[i], nullptr);
        vkFreeMemory(device, depthImageMemories[i], nullptr);
        vkDestroyImage(device, depthImages[i], nullptr);
    }
    for (auto framebuffer : offScreenFramebuffers)
    {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    offScreenFramebuffers.clear();
    for (size_t i = 0; i < offScreenImages.size(); i++)
    {
        vkDestroyImageView(device, offScreenImageViews[i], nullptr);
        vkFreeMemory(device, offScreenImageMemories[i], nullptr);
        vkDestroyImage(device, offScreenImages[i], nullptr);
    }
    vkDestroySampler(device, offScreenSampler, nullptr);
}

void RenderTarget::cleanupPresentTarget()
{
    vkDeviceWaitIdle(device);
    for (auto framebuffer : framebuffers)
    {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    framebuffers.clear();
}

void RenderTarget::cleanup()
{
    cleanupOffScreenTarget();
    cleanupPresentTarget();
}

void RenderTarget::createDepthResources(VkExtent2D imageExtent)
{
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();
    depthImages.resize(swapChainManager->getSwapChainImageViews().size());
    depthImageMemories.resize(depthImages.size());
    depthImageViews.resize(depthImages.size());
    for (size_t i = 0; i < depthImages.size(); i++)
    {
        VkFormat depthFormat = vulkanUtils.findDepthFormat(physicalDevice);
        vulkanUtils.createImage(device, physicalDevice, imageExtent.width, imageExtent.height, depthFormat,
                                VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImages[i], depthImageMemories[i]);
        depthImageViews[i] =
            vulkanUtils.createImageView(device, depthImages[i], depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    }
}

void RenderTarget::createOffScreenResources(VkExtent2D imageExtent)
{
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();
    offScreenImages.resize(swapChainManager->getSwapChainImageViews().size());
    offScreenImageMemories.resize(offScreenImages.size());
    offScreenImageViews.resize(offScreenImages.size());
    for (size_t i = 0; i < offScreenImages.size(); i++)
    {
        // VkFormat offScreenFormat = VK_FORMAT_R8G8B8A8_UNORM;
        VkFormat offScreenFormat = swapChainManager->getSwapChainImageFormat();
        vulkanUtils.createImage(device, physicalDevice, imageExtent.width, imageExtent.height, offScreenFormat,
                                VK_IMAGE_TILING_OPTIMAL,
                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, offScreenImages[i], offScreenImageMemories[i]);
        offScreenImageViews[i] =
            vulkanUtils.createImageView(device, offScreenImages[i], offScreenFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void RenderTarget::createFramebuffers(const std::vector<VkImageView>& swapChainImageViews, VkRenderPass renderPass)
{
    framebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++)
    {
        std::array<VkImageView, 1> attachments = {
            swapChainImageViews[i],
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainManager->getSwapChainExtent().width;
        framebufferInfo.height = swapChainManager->getSwapChainExtent().height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void RenderTarget::createOffScreenFramebuffers(VkRenderPass renderPass, VkExtent2D imageExtent)
{
    offScreenFramebuffers.resize(swapChainManager->getSwapChainImageViews().size());

    for (size_t i = 0; i < offScreenFramebuffers.size(); i++)
    {
        std::array<VkImageView, 2> attachments = {offScreenImageViews[i], depthImageViews[i]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = imageExtent.width;
        framebufferInfo.height = imageExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &offScreenFramebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create off-screen framebuffer!");
        }
    }
}

void RenderTarget::createOffScreenSampler()
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &offScreenSampler) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create off-screen image sampler!");
    }
}