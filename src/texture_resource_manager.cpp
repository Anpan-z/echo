#include "texture_resource_manager.hpp"
#include "vulkan_utils.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h> // 用于加载 HDR 图像
#include <stdexcept>

void TextureResourceManager::init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue graphicsQueue, CommandManager& commandManager) {
    this->device = device;
    this->physicalDevice = physicalDevice;
    this->graphicsQueue = graphicsQueue;
    this->commandManager = &commandManager;

    createEnvironmentMap();
    // createIrradianceMap();
}

void TextureResourceManager::cleanup() {
    vkDestroyImageView(device, sourceHDRImageView, nullptr);
    vkDestroyImage(device, sourceHDRImage, nullptr);
    vkFreeMemory(device, sourceHDRImageMemory, nullptr);
    for (size_t i = 0; i < environmentMapFaceImageViews.size(); i++) {
        vkDestroyImageView(device, environmentMapFaceImageViews[i], nullptr);
    }

    vkDestroyImageView(device, environmentMapImageView, nullptr);
    vkDestroyImage(device, environmentMapImage, nullptr);
    vkFreeMemory(device, environmentMapImageMemory, nullptr);

//     vkDestroyImageView(device, irradianceMapImageView, nullptr);
//     vkDestroyImage(device, irradianceMapImage, nullptr);
//     vkFreeMemory(device, irradianceMapImageMemory, nullptr);

//     vkDestroyImageView(device, prefilteredMapImageView, nullptr);
//     vkDestroyImage(device, prefilteredMapImage, nullptr);
//     vkFreeMemory(device, prefilteredMapImageMemory, nullptr);

//     vkDestroyImageView(device, brdfLUTImageView, nullptr);
//     vkDestroyImage(device, brdfLUTImage, nullptr);
//     vkFreeMemory(device, brdfLUTImageMemory, nullptr);
}

VkImageView TextureResourceManager::loadHDRTexture(const std::string& hdrPath) {
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();

    int texWidth, texHeight, texChannels;
    float* pixels = stbi_loadf(hdrPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4 * sizeof(float);;
    
    if (!pixels) {
        throw std::runtime_error("Failed to load HDR texture: " + hdrPath);
    }
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    vulkanUtils.createBuffer(device, physicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);
    
    stbi_image_free(pixels);
    
    vulkanUtils.createImage(device, physicalDevice, texWidth, texHeight, VK_FORMAT_R32G32B32A32_SFLOAT , VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sourceHDRImage, sourceHDRImageMemory);
    VkCommandPool commandPool = commandManager->getCommandPool();
    vulkanUtils.transitionImageLayout(device, commandPool, graphicsQueue, sourceHDRImage, VK_FORMAT_R32G32B32A32_SFLOAT , VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vulkanUtils.copyBufferToImage(device, commandPool, graphicsQueue, stagingBuffer, sourceHDRImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    vulkanUtils.transitionImageLayout(device, commandPool, graphicsQueue, sourceHDRImage, VK_FORMAT_R32G32B32A32_SFLOAT , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    sourceHDRImageView = vulkanUtils.createImageView(device, sourceHDRImage, VK_FORMAT_R32G32B32A32_SFLOAT , VK_IMAGE_ASPECT_COLOR_BIT);
    
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void TextureResourceManager::createEnvironmentMap() {
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();

    // 1. 创建环境贴图的 VkImage 和 VkImageView
    const uint32_t environmentMapSize = 512; // 通常环境贴图的分辨率较高，例如 512x512 或 1024x1024
    vulkanUtils.createImage(
        device,
        physicalDevice,
        environmentMapSize,
        environmentMapSize,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        environmentMapImage,
        environmentMapImageMemory,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, 
        6
    );

    environmentMapImageView = vulkanUtils.createImageView(
        device,
        environmentMapImage,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_VIEW_TYPE_CUBE,
        6 // 立方体贴图的层数
    );
    // 2. 创建每个面对应的 VkImageView
    for (uint32_t i = 0; i < 6; i++) {
        environmentMapFaceImageViews[i] = vulkanUtils.createImageView(
            device,
            environmentMapImage,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_VIEW_TYPE_2D, // 每个面是一个 2D 图像
            1,                     // 每个 ImageView 只访问一个层
            i                      // 指定访问的层索引（对应立方体贴图的面）
        );
    }
    
}

void TextureResourceManager::createIrradianceMap() {
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();

    // 1. 创建辐照度贴图的 VkImage 和 VkImageView
    const uint32_t irradianceMapSize = 32; // 通常辐照度贴图的分辨率较低，例如 32x32
    vulkanUtils.createImage(
        device,
        physicalDevice,
        irradianceMapSize,
        irradianceMapSize,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        irradianceMapImage,
        irradianceMapImageMemory,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, 
        6
    );

    irradianceMapImageView = vulkanUtils.createImageView(
        device,
        irradianceMapImage,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_VIEW_TYPE_CUBE,
        6 // 立方体贴图的层数
    );
}