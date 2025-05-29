#pragma once

#include "command_manager.hpp"
#include "swap_chain_manager.hpp"
#include <vector>
#include <vulkan/vulkan.h>

class SVGFilterImageRecreateObserver
{
  public:
    virtual void onSVGFilterImageRecreated() = 0;
    virtual ~SVGFilterImageRecreateObserver() = default;
};

class SVGFilterResourceManager
{
  public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue graphicsQueue,
              SwapChainManager& swapChainManager, CommandManager& commandManager);

    void cleanup();

    void recreateDenoisedOutputImages();

    std::vector<VkImageView> getDenoisedOutputImageView() const
    {
        return denoisedOutputImageViews;
    };

    std::vector<VkImage> getDenoisedOutputImage() const
    {
        return denoisedOutputImages;
    };

    VkSampler getDenoiserInputSampler() const
    {
        return denoiserInputSampler;
    }

    VkExtent2D getOutputExtent() const
    {
        return outPutExtent;
    }

    void addSVGFilterImageRecreateObserver(SVGFilterImageRecreateObserver* observer)
    {
        svgFilterImageRecreateObservers.push_back(observer);
    }

  private:
    void createDenoisedOutputImages();
    void createSamplers();

    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkQueue graphicsQueue;
    VkExtent2D outPutExtent;
    CommandManager* commandManager = nullptr;
    SwapChainManager* swapChainManager = nullptr;
    uint32_t imageWidth;
    uint32_t imageHeight;
    uint32_t imageCount;

    std::vector<VkImage> denoisedOutputImages;
    std::vector<VkDeviceMemory> denoisedOutputImageMemories;
    std::vector<VkImageView> denoisedOutputImageViews;

    std::vector<SVGFilterImageRecreateObserver*> svgFilterImageRecreateObservers;

    // 采样器 (可以是一个通用的，或为每个输入纹理单独创建)
    VkSampler denoiserInputSampler;
    // VkSampler pathTracedColorSampler;
    // VkSampler gbufferNormalSampler;
    // VkSampler gbufferDepthSampler;
};