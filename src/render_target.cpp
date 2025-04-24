#include "render_target.hpp"
#include "vulkan_utils.hpp"
#include <stdexcept>
#include <array>

void RenderTarget::init(VkDevice device, VkPhysicalDevice physicalDevice, SwapChainManager& swapChainManager, VkRenderPass renderPass) {
    this->device = device;
    this->physicalDevice = physicalDevice;
    this->swapChainManager = &swapChainManager;

    createDepthResources();
    createFramebuffers(swapChainManager.getSwapChainImageViews(), renderPass);
}

void RenderTarget::cleanup() {
    vkDeviceWaitIdle(device);
    for (auto framebuffer : framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    framebuffers.clear();

    if (depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, depthImageView, nullptr);
        depthImageView = VK_NULL_HANDLE;
    }

    if (depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, depthImage, nullptr);
        depthImage = VK_NULL_HANDLE;
    }

    if (depthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, depthImageMemory, nullptr);
        depthImageMemory = VK_NULL_HANDLE;
    }
}

void RenderTarget::createDepthResources() {
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();
    VkFormat depthFormat = vulkanUtils.findDepthFormat(physicalDevice);
    vulkanUtils.createImage(device, physicalDevice, swapChainManager->getSwapChainExtent().width, swapChainManager->getSwapChainExtent().height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
    depthImageView = vulkanUtils.createImageView(device, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

}

void RenderTarget::createFramebuffers(const std::vector<VkImageView>& swapChainImageViews, VkRenderPass renderPass) {
    framebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            swapChainImageViews[i],
            depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainManager->getSwapChainExtent().width;
        framebufferInfo.height = swapChainManager->getSwapChainExtent().height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}
