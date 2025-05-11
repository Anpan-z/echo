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
    createIrradianceMap();
    createPrefilteredMap();

    createEnvironmentMapSampler();
    createIrradianceMapSampler();
    createPrefilteredMapSampler();
    createBRDFLUTSampler();
}

void TextureResourceManager::cleanup() {
    vkDestroyImageView(device, sourceHDRImageView, nullptr);
    vkDestroyImage(device, sourceHDRImage, nullptr);
    vkFreeMemory(device, sourceHDRImageMemory, nullptr);
    
    vkDestroyImageView(device, environmentMapImageView, nullptr);
    vkDestroyImage(device, environmentMapImage, nullptr);
    vkFreeMemory(device, environmentMapImageMemory, nullptr);
    for (size_t i = 0; i < environmentMapFaceImageViews.size(); i++) {
        vkDestroyImageView(device, environmentMapFaceImageViews[i], nullptr);
    }
    
    vkDestroyImageView(device, irradianceMapImageView, nullptr);
    vkDestroyImage(device, irradianceMapImage, nullptr);
    vkFreeMemory(device, irradianceMapImageMemory, nullptr);
    for (size_t i = 0; i < irradianceMapFaceImageViews.size(); i++) {
        vkDestroyImageView(device, irradianceMapFaceImageViews[i], nullptr);
    }
    
    vkDestroyImageView(device, prefilteredMapImageView, nullptr);
    vkDestroyImage(device, prefilteredMapImage, nullptr);
    vkFreeMemory(device, prefilteredMapImageMemory, nullptr);
    for (size_t mipLevel = 0; mipLevel < prefilteredMapFaceImageViews.size(); mipLevel++) {
        for (size_t face = 0; face < 6; face++) {
            vkDestroyImageView(device, prefilteredMapFaceImageViews[mipLevel][face], nullptr);
        }
    }


//     vkDestroyImageView(device, brdfLUTImageView, nullptr);
//     vkDestroyImage(device, brdfLUTImage, nullptr);
//     vkFreeMemory(device, brdfLUTImageMemory, nullptr);
    vkDestroySampler(device, environmentMapSampler, nullptr);
    vkDestroySampler(device, irradianceMapSampler, nullptr);
    vkDestroySampler(device, prefilteredMapSampler, nullptr);
    vkDestroySampler(device, brdfLUTSampler, nullptr);
}

VkImageView TextureResourceManager::loadHDRTexture(const std::string& hdrPath) {
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();

    int texWidth, texHeight, texChannels;
    stbi_set_flip_vertically_on_load(false);
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
    for (uint32_t i = 0; i < 6; i++) {
        irradianceMapFaceImageViews[i] = vulkanUtils.createImageView(
            device,
            irradianceMapImage,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_VIEW_TYPE_2D, // 每个面是一个 2D 图像
            1,                     // 每个 ImageView 只访问一个层
            i                      // 指定访问的层索引（对应立方体贴图的面）
        );
    }
}

void TextureResourceManager::createPrefilteredMap() {
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();

    // 1. 创建预过滤环境贴图的 VkImage 和 VkImageView
    const uint32_t prefilteredMapSize = 512; // 通常预过滤环境贴图的分辨率较高，例如 512x512 或 1024x1024
    const uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(prefilteredMapSize))) + 1; // 计算 Mipmap 级别
    vulkanUtils.createImage(
        device,
        physicalDevice,
        prefilteredMapSize,
        prefilteredMapSize,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        prefilteredMapImage,
        prefilteredMapImageMemory,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, 
        6, // 立方体贴图的层数
        mipLevels // 设置 Mipmap 级别
    );

    prefilteredMapImageView = vulkanUtils.createImageView(
        device,
        prefilteredMapImage,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_VIEW_TYPE_CUBE,
        6, // 立方体贴图的层数
        0,
        mipLevels, // 设置 Mipmap 级别
        0 // 起始 Mipmap 级别
    );

    // 2. 初始化数据结构
    prefilteredMapFaceImageViews.resize(mipLevels);

    // 3. 为每个 Mipmap 等级的每个面创建单独的 VkImageView
    for (uint32_t mipLevel = 0; mipLevel < mipLevels; ++mipLevel) {
        for (uint32_t face = 0; face < 6; ++face) {
            prefilteredMapFaceImageViews[mipLevel][face] = vulkanUtils.createImageView(
                device,
                prefilteredMapImage,
                VK_FORMAT_R32G32B32A32_SFLOAT,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_VIEW_TYPE_2D, // 每个面是一个 2D 图像
                1,                     // 每个 ImageView 只访问一个层
                face,                  // 指定访问的层索引（对应立方体贴图的面）
                1,                     // 只访问当前 Mipmap 等级
                mipLevel               // 起始 Mipmap 等级
            );
        }
    }
}

// 环境贴图采样器 (Skybox):
// 目的：渲染天空盒
// 各向异性：启用 (VK_TRUE)
// 各向异性等级：16.0f
// Mipmap：禁用 (maxLod = 0.0f)
// 过滤模式：线性
void TextureResourceManager::createEnvironmentMapSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR; // 放大过滤
    samplerInfo.minFilter = VK_FILTER_LINEAR; // 缩小过滤
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; // Mipmap 线性过滤
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // U 方向边界模式
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // V 方向边界模式
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // W 方向边界模式
    samplerInfo.mipLodBias = 0.0f; // Mipmap 偏移
    samplerInfo.anisotropyEnable = VK_TRUE; // 启用各向异性过滤
    samplerInfo.maxAnisotropy = 16; // 最大各向异性等级
    samplerInfo.compareEnable = VK_FALSE; // 不启用比较操作
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.minLod = 0.0f; // 最小 LOD
    samplerInfo.maxLod = 0.0f; // 最大 LOD - 不使用 Mipmap
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // 边界颜色
    samplerInfo.unnormalizedCoordinates = VK_FALSE; // 使用标准化纹理坐标

    
    if (vkCreateSampler(device, &samplerInfo, nullptr, &environmentMapSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create sampler!");
    }
}

// 辐射照度采样器 (Irradiance Map):
// 目的：环境光照计算
// 各向异性：禁用 (VK_FALSE)
// 各向异性等级：1
// Mipmap：禁用 (maxLod = 0.0f)
// 过滤模式：线性
void TextureResourceManager::createIrradianceMapSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR; // 放大过滤
    samplerInfo.minFilter = VK_FILTER_LINEAR; // 缩小过滤
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; // Mipmap 线性过滤
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // U 方向边界模式
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // V 方向边界模式
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // W 方向边界模式
    samplerInfo.mipLodBias = 0.0f; // Mipmap 偏移
    samplerInfo.anisotropyEnable = VK_FALSE; // 关闭各向异性过滤
    samplerInfo.maxAnisotropy = 1; // 最大各向异性等级
    samplerInfo.compareEnable = VK_FALSE; // 不启用比较操作
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.minLod = 0.0f; // 最小 LOD
    samplerInfo.maxLod = 0.0f; // 最大 LOD - 不使用 Mipmap
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // 边界颜色
    samplerInfo.unnormalizedCoordinates = VK_FALSE; // 使用标准化纹理坐标

    
    if (vkCreateSampler(device, &samplerInfo, nullptr, &irradianceMapSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create sampler!");
    }
}

// 预过滤环境贴图采样器 (Prefiltered Environment Map):
// 目的：基于粗糙度的环境反射
// 各向异性：禁用 (VK_FALSE)
// 各向异性等级：1
// Mipmap：启用 (maxLod = VK_LOD_CLAMP_NONE)
// 过滤模式：线性
// 特点：每个 Mipmap 级别代表不同粗糙度
void TextureResourceManager::createPrefilteredMapSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR; // 放大过滤
    samplerInfo.minFilter = VK_FILTER_LINEAR; // 缩小过滤
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; // Mipmap 线性过滤 - 用于不同粗糙度
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // U 方向边界模式
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // V 方向边界模式
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // W 方向边界模式
    samplerInfo.mipLodBias = 0.0f; // Mipmap 偏移
    samplerInfo.anisotropyEnable = VK_FALSE; // 关闭各向异性过滤
    samplerInfo.maxAnisotropy = 1.0f; // 最大各向异性等级
    samplerInfo.compareEnable = VK_FALSE; // 不启用比较操作
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.minLod = 0.0f; // 最小 LOD
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE; // 最大 LOD - 使用所有 Mipmap 级别
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // 边界颜色
    samplerInfo.unnormalizedCoordinates = VK_FALSE; // 使用标准化纹理坐标

    if (vkCreateSampler(device, &samplerInfo, nullptr, &prefilteredMapSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create prefiltered map sampler!");
    }
}

// BRDF LUT 采样器 (BRDF Look-Up Table):
// 目的：预计算 BRDF 积分
// 各向异性：禁用 (VK_FALSE)
// 各向异性等级：1
// Mipmap：禁用 (maxLod = 0.0f)
// 过滤模式：线性
// 特点：2D 纹理，用于 PBR 光照计算
void TextureResourceManager::createBRDFLUTSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR; // 放大过滤
    samplerInfo.minFilter = VK_FILTER_LINEAR; // 缩小过滤
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST; // Mipmap 线性过滤 - 用于不同粗糙度
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // U 方向边界模式
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // V 方向边界模式
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // W 方向边界模式
    samplerInfo.mipLodBias = 0.0f; // Mipmap 偏移
    samplerInfo.anisotropyEnable = VK_FALSE; // 关闭各向异性过滤
    samplerInfo.maxAnisotropy = 1.0f; // 最大各向异性等级
    samplerInfo.compareEnable = VK_FALSE; // 不启用比较操作
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.minLod = 0.0f; // 最小 LOD
    samplerInfo.maxLod = 0.0f; // 最大 LOD - 不使用 Mipmap
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // 边界颜色
    samplerInfo.unnormalizedCoordinates = VK_FALSE; // 使用标准化纹理坐标

    
    if (vkCreateSampler(device, &samplerInfo, nullptr, &brdfLUTSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create BRDF LUT sampler!");
    }
}