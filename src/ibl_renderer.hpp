#pragma once

#include <vulkan/vulkan.h>
#include <tuple>
#include "texture_resource_manager.hpp"


class IBLRenderer {
public:
    void init(VkDevice device, VkQueue graphicsQueue, CommandManager& commandManager, TextureResourceManager& resourceManager);
    void cleanup();
    void generateEnvironmentMap(TextureResourceManager& resourceManager);
    void generateIrradianceMap(TextureResourceManager& resourceManager);

private:
    VkDevice device;
    VkQueue graphicsQueue;
    CommandManager* commandManager;

    VkSampler environmentMapSampler;
    VkDescriptorPool descriptorPool;

    VkDescriptorSetLayout descriptorSetLayout;

    VkRenderPass createRenderPass(VkFormat format);
    VkFramebuffer createFramebuffer(VkImageView& imageView, VkRenderPass renderPass, uint32_t width, uint32_t height);
    std::tuple<VkPipeline, VkPipelineLayout> createPipeline(VkRenderPass renderPass, uint32_t size, std::string vertex_shader_code_path, std::string fragment_shader_code_path);
    void createDescriptorSetLayout();
    VkDescriptorSet createDescriptorSet(VkImageView environmentMapImageView);
    void createDescriptorPool();
    //void createDescriptorSetLayout(VkDescriptorSetLayout& descriptorSetLayout);
    void createSampler();
};