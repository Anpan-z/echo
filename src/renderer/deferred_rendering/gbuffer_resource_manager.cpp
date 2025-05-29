#include "gbuffer_resource_manager.hpp"
#include "vulkan_utils.hpp"

void GBufferResourceManager::init(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass,
                                  SwapChainManager& swapChainManager)
{
    this->device = device;
    this->physicalDevice = physicalDevice;
    this->width = swapChainManager.getSwapChainExtent().width;
    this->height = swapChainManager.getSwapChainExtent().height;
    this->imageCount = swapChainManager.getSwapChainImages().size();

    // 创建 G-Buffer 附件
    createAttachment(colorAttachments, swapChainManager.getSwapChainImageFormat(),
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    createAttachment(normalAttachments, VK_FORMAT_R16G16B16A16_SFLOAT,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    createAttachment(positionAttachments, VK_FORMAT_R16G16B16A16_SFLOAT,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    createAttachment(depthAttachments, VK_FORMAT_D32_SFLOAT,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_IMAGE_ASPECT_DEPTH_BIT);

    // 创建帧缓冲
    createFramebuffer(renderPass);
}

void GBufferResourceManager::cleanup()
{
    // 销毁帧缓冲
    for (auto framebuffer : framebuffers)
    {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    // 销毁 G-Buffer 附件
    auto destroyAttachment = [this](GBufferAttachment& attachment) {
        vkDestroyImageView(device, attachment.view, nullptr);
        vkDestroyImage(device, attachment.image, nullptr);
        vkFreeMemory(device, attachment.memory, nullptr);
    };
    for (int index = 0; index < positionAttachments.size(); ++index)
    {
        destroyAttachment(positionAttachments[index]);
        destroyAttachment(colorAttachments[index]);
        destroyAttachment(normalAttachments[index]);
        destroyAttachment(depthAttachments[index]);
    }
}

void GBufferResourceManager::createAttachment(std::vector<GBufferAttachment>& attachment, VkFormat format,
                                              VkImageUsageFlags usage, VkImageAspectFlags aspectFlags)
{
    attachment.resize(imageCount);
    for (uint32_t index = 0; index < imageCount; ++index)
    {
        attachment[index].format = format;
        VulkanUtils& vulkanUtils = VulkanUtils::getInstance();
        vulkanUtils.createImage(device, physicalDevice, width, height, format, VK_IMAGE_TILING_OPTIMAL, usage,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, attachment[index].image, attachment[index].memory);
        attachment[index].view = vulkanUtils.createImageView(device, attachment[index].image, format, aspectFlags);
    }
}

void GBufferResourceManager::createFramebuffer(VkRenderPass renderPass)
{
    framebuffers.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        std::vector<VkImageView> attachments = {colorAttachments[i].view, normalAttachments[i].view,
                                                positionAttachments[i].view, depthAttachments[i].view};

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = width;
        framebufferInfo.height = height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create G-Buffer framebuffer!");
        }
    }
}