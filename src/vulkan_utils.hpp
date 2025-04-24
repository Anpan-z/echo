#pragma once

#include <stdexcept>
#include <vector>
#include <vulkan/vulkan.h>
#include <fstream>
#include <iostream>

class VulkanUtils
{
  public:
    // 获取单例实例
    static VulkanUtils &getInstance();

    // 禁用拷贝构造和赋值操作
    VulkanUtils(const VulkanUtils &) = delete;
    VulkanUtils &operator=(const VulkanUtils &) = delete;

    // Vulkan 工具函数
    void createImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format,
                     VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image,
                     VkDeviceMemory &imageMemory);

    VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

    void transitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkImage image,
                               VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);

    VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat> &candidates, VkImageTiling tiling,
                                 VkFormatFeatureFlags features);

    VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);

    void endSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue,
                               VkCommandBuffer commandBuffer);
    void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory);

    void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer srcBuffer,
                    VkBuffer dstBuffer, VkDeviceSize size);

    std::string readFileToString(const std::string& filename);

    VkShaderModule createShaderModule(VkDevice device, VkShaderModuleCreateInfo& createInfo);
  private:
    // 私有构造函数，确保外部无法实例化
    VulkanUtils() = default;
};