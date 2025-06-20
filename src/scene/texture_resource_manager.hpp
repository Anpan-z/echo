#pragma once

#include "command_manager.hpp"
#include <array>
#include <string>
#include <vulkan/vulkan.h>

class TextureResourceManager
{
  public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue graphicsQueue, CommandManager& commandManager);
    void cleanup();

    VkImageView loadHDRTexture(const std::string& hdrPath);
    VkImageView createTexture(const std::string& texturePath);

    void createEnvironmentMap();
    void createIrradianceMap();
    void createPrefilteredMap();
    void createBRDFLUT();

    const VkImageView& getSourceHDRImageView() const
    {
        return sourceHDRImageView;
    }
    const VkImageView& getEnvironmentMapImageView() const
    {
        return environmentMapImageView;
    }
    const std::array<VkImageView, 6>& getEnvironmentMapFaceImageViews() const
    {
        return environmentMapFaceImageViews;
    }
    const VkImageView& getIrradianceMapImageView() const
    {
        return irradianceMapImageView;
    }
    const std::array<VkImageView, 6>& getIrradianceMapFaceImageViews() const
    {
        return irradianceMapFaceImageViews;
    }
    const VkImageView& getPrefilteredMapImageView() const
    {
        return prefilteredMapImageView;
    }
    const std::vector<std::array<VkImageView, 6>>& getPrefilteredMapFaceImageViews() const
    {
        return prefilteredMapFaceImageViews;
    }
    const VkImageView& getBRDFLUTImageView() const
    {
        return brdfLUTImageView;
    }

    const VkSampler& getEnvironmentMapSampler() const
    {
        return environmentMapSampler;
    }
    const VkSampler& getIrradianceMapSampler() const
    {
        return irradianceMapSampler;
    }
    const VkSampler& getPrefilteredMapSampler() const
    {
        return prefilteredMapSampler;
    }
    const VkSampler& getBRDFLUTSampler() const
    {
        return brdfLUTSampler;
    }

  private:
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    CommandManager* commandManager;
    VkQueue graphicsQueue;

    VkImage sourceHDRImage;
    VkDeviceMemory sourceHDRImageMemory;
    VkImageView sourceHDRImageView;

    VkImage environmentMapImage;
    VkDeviceMemory environmentMapImageMemory;
    VkImageView environmentMapImageView;
    std::array<VkImageView, 6> environmentMapFaceImageViews;

    VkImage irradianceMapImage;
    VkDeviceMemory irradianceMapImageMemory;
    VkImageView irradianceMapImageView;
    std::array<VkImageView, 6> irradianceMapFaceImageViews;

    VkImage prefilteredMapImage;
    VkDeviceMemory prefilteredMapImageMemory;
    VkImageView prefilteredMapImageView;
    std::vector<std::array<VkImageView, 6>> prefilteredMapFaceImageViews;

    VkImage brdfLUTImage;
    VkDeviceMemory brdfLUTImageMemory;
    VkImageView brdfLUTImageView;

    VkSampler environmentMapSampler;
    VkSampler irradianceMapSampler;
    VkSampler prefilteredMapSampler;
    VkSampler brdfLUTSampler;

    void createEnvironmentMapSampler();
    void createIrradianceMapSampler();
    void createPrefilteredMapSampler();
    void createBRDFLUTSampler();
};