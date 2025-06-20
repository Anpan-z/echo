#pragma once

#include "texture_resource_manager.hpp"
#include <tuple>
#include <vulkan/vulkan.h>

class IBLRenderer
{
  public:
    void init(VkDevice device, VkQueue graphicsQueue, CommandManager& commandManager,
              TextureResourceManager& resourceManager);
    void cleanup();
    void generateEnvironmentMap(TextureResourceManager& resourceManager);
    void generateIrradianceMap(TextureResourceManager& resourceManager);
    void generatePrefilteredMap(TextureResourceManager& resourceManager);
    void generateBRDFLUT(TextureResourceManager& resourceManager);

  private:
    VkDevice device;
    VkQueue graphicsQueue;
    CommandManager* commandManager;

    VkSampler environmentMapSampler;
    VkDescriptorPool descriptorPool;

    VkDescriptorSetLayout descriptorSetLayout;

    VkRenderPass createRenderPass(VkFormat format);
    VkFramebuffer createFramebuffer(VkImageView& imageView, VkRenderPass renderPass, uint32_t width, uint32_t height);
    std::tuple<VkPipeline, VkPipelineLayout> createPipeline(VkRenderPass renderPass, uint32_t size,
                                                            std::string vertex_shader_code_path,
                                                            std::string fragment_shader_code_path);
    void createDescriptorSetLayout();
    VkDescriptorSet createDescriptorSet(VkImageView imageView, VkSampler sampler);
    void createDescriptorPool();
    // void createDescriptorSetLayout(VkDescriptorSetLayout& descriptorSetLayout);
    void createSampler();
};