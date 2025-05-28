#pragma once

#include "swap_chain_manager.hpp"
#include <vector>
#include <vulkan/vulkan.h>

// 表示 G-Buffer 中的一个附件（颜色、法线或深度）
struct GBufferAttachment
{
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
};

class GBufferResourceManager
{
  public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass,
              SwapChainManager& swapChainManager);

    void cleanup();

    const GBufferAttachment& getColorAttachment(uint32_t index) const
    {
        return colorAttachments[index];
    }
    const GBufferAttachment& getNormalAttachment(uint32_t index) const
    {
        return normalAttachments[index];
    }
    const GBufferAttachment& getDepthAttachment(uint32_t index) const
    {
        return depthAttachments[index];
    }
    const GBufferAttachment& getPositionAttachment(uint32_t index) const
    {
        return positionAttachments[index];
    }

    VkFramebuffer getFramebuffer(uint32_t index) const
    {
        return framebuffers[index];
    }

  private:
    VkDevice device;
    VkPhysicalDevice physicalDevice;

    uint32_t width;
    uint32_t height;
    uint32_t imageCount;

    std::vector<GBufferAttachment> positionAttachments; // 世界空间位置
    std::vector<GBufferAttachment> colorAttachments;    // 漫反射颜色
    std::vector<GBufferAttachment> normalAttachments;   // 法线
    std::vector<GBufferAttachment> depthAttachments;    // 深度

    std::vector<VkFramebuffer> framebuffers;

    void createAttachment(std::vector<GBufferAttachment>& attachment, VkFormat format, VkImageUsageFlags usage,
                          VkImageAspectFlags aspectFlags);
    void createFramebuffer(VkRenderPass renderPass);
};