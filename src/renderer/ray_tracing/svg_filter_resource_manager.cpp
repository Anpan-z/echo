#include "svg_filter_resource_manager.hpp"
#include "vulkan_utils.hpp"

void SVGFilterResourceManager::init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue graphicsQueue,
                                    SwapChainManager& swapChainManager, CommandManager& commandManager)
{
    this->device = device;
    this->physicalDevice = physicalDevice;
    this->graphicsQueue = graphicsQueue;
    this->swapChainManager = &swapChainManager;
    this->outPutExtent = swapChainManager.getSwapChainExtent();
    this->imageWidth = swapChainManager.getSwapChainExtent().width;
    this->imageHeight = swapChainManager.getSwapChainExtent().height;
    this->imageCount = swapChainManager.getSwapChainImages().size();
    this->commandManager = &commandManager;

    createDenoisedOutputImages();
    createSamplers();
}

void SVGFilterResourceManager::cleanup()
{
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        if (denoisedOutputImageViews.size() > i && denoisedOutputImageViews[i] != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, denoisedOutputImageViews[i], nullptr);
        }
        if (denoisedOutputImages.size() > i && denoisedOutputImages[i] != VK_NULL_HANDLE)
        {
            vkDestroyImage(device, denoisedOutputImages[i], nullptr);
        }
        if (denoisedOutputImageMemories.size() > i && denoisedOutputImageMemories[i] != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, denoisedOutputImageMemories[i], nullptr);
        }
    }
    denoisedOutputImageViews.clear();
    denoisedOutputImages.clear();
    denoisedOutputImageMemories.clear();

    if (denoiserInputSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(device, denoiserInputSampler, nullptr);
        denoiserInputSampler = VK_NULL_HANDLE;
    }
}

void SVGFilterResourceManager::recreateDenoisedOutputImages(VkExtent2D imageExtent)
{
    vkDeviceWaitIdle(device); // 等待设备空闲
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        vkDestroyImageView(device, denoisedOutputImageViews[i], nullptr);
        vkDestroyImage(device, denoisedOutputImages[i], nullptr);
        vkFreeMemory(device, denoisedOutputImageMemories[i], nullptr);
    }
    this->outPutExtent = imageExtent;
    this->imageWidth = imageExtent.width;
    this->imageHeight = imageExtent.height;
    createDenoisedOutputImages();

    for (auto observer : svgFilterImageRecreateObservers)
    {
        observer->onSVGFilterImageRecreated(); // 通知观察者
    }
}

void SVGFilterResourceManager::createDenoisedOutputImages()
{
    denoisedOutputImages.resize(imageCount);
    denoisedOutputImageMemories.resize(imageCount);
    denoisedOutputImageViews.resize(imageCount);

    VulkanUtils& vulkanUtils = VulkanUtils::getInstance(); // 获取辅助类实例

    VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    // 如果只是写入，然后直接复制到交换链，可以只有 VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT

    for (uint32_t i = 0; i < imageCount; ++i)
    {
        vulkanUtils.createImage(device, physicalDevice, imageWidth, imageHeight, format, VK_IMAGE_TILING_OPTIMAL, usage,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, denoisedOutputImages[i],
                                denoisedOutputImageMemories[i]);

        denoisedOutputImageViews[i] =
            vulkanUtils.createImageView(device, denoisedOutputImages[i], format, VK_IMAGE_ASPECT_COLOR_BIT);

        // 初始布局转换
        vulkanUtils.transitionImageLayout(device, commandManager->getCommandPool(), graphicsQueue,
                                          denoisedOutputImages[i], format, VK_IMAGE_LAYOUT_UNDEFINED,
                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

void SVGFilterResourceManager::createSamplers()
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // 或者 REPEAT 等
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE; // 根据需要开启
    // samplerInfo.maxAnisotropy = physicalDeviceProperties.limits.maxSamplerAnisotropy; // 如果开启
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; // 如果有 mipmaps
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f; // 或者 mipmap 级别数

    if (vkCreateSampler(device, &samplerInfo, nullptr, &denoiserInputSampler) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create common sampler for SVG filter!");
    }
}