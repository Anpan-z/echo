#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <array>
#include "command_manager.hpp"

class TextureResourceManager {
public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue graphicsQueue, CommandManager& commandManager);
    void cleanup();

    VkImageView loadHDRTexture(const std::string& hdrPath);
    VkImageView createTexture(const std::string& texturePath);

    void createEnvironmentMap();
    void createIrradianceMap();
    void createPrefilteredMap(VkImage environmentMapImage);
    void createBRDFLUT();

    const VkImageView& getSourceHDRImageView() const { return sourceHDRImageView; }
    const VkImageView& getEnvironmentMapImageView() const { return environmentMapImageView; }
    const std::array<VkImageView, 6>& getEnvironmentMapFaceImageViews() const { return environmentMapFaceImageViews; }
    const VkImageView& getIrradianceMapImageView() const { return irradianceMapImageView; }
    const VkImageView& getPrefilteredMapImageView() const { return prefilteredMapImageView; }
    const VkImageView& getBRDFLUTImageView() const { return brdfLUTImageView; }

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

    VkImage prefilteredMapImage;
    VkDeviceMemory prefilteredMapImageMemory;
    VkImageView prefilteredMapImageView;

    VkImage brdfLUTImage;
    VkDeviceMemory brdfLUTImageMemory;
    VkImageView brdfLUTImageView;
};