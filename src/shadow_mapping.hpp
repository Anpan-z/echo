#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "vertex_resource_manager.hpp"


const uint32_t SHADOW_MAP_WIDTH = 2048;
const uint32_t SHADOW_MAP_HEIGHT = 2048;

struct ShadowUBO {
    glm::mat4 lightSpaceMatrix;
};

class ShadowMapping {
public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice, VertexResourceManager& vertexResourceManager, std::vector<VkCommandBuffer>&& shadowCommandBuffers);
    void cleanup();

    // void updateShadowUniformBuffer(uint32_t currentFrame, const glm::mat4& lightSpaceMatrix);
    void updateShadowUniformBuffer(uint32_t currentFrame);
    VkCommandBuffer recordShadowCommandBuffer(uint32_t currentFrame);

    VkDescriptorSetLayout getShadowDescriptorSetLayout(){return shadowDescriptorSetLayout;};
    VkImageView getShadowDepthImageView(){return shadowDepthImageView;};
    VkSampler getShadowSampler(){return shadowSampler;};
    VkCommandBuffer getCommandBuffer(uint32_t index) { return shadowCommandBuffers[index]; }

private:
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    std::vector<VkCommandBuffer> shadowCommandBuffers;
    VkDescriptorPool shadowDescriptorPool;

    VkImage shadowDepthImage;
    VkDeviceMemory shadowDepthImageMemory;
    VkImageView shadowDepthImageView;
    VkFramebuffer shadowFramebuffer;
    VkRenderPass shadowRenderPass;
    VkPipeline shadowPipeline;
    VkPipelineLayout shadowPipelineLayout;

    VkDescriptorSetLayout shadowDescriptorSetLayout;

    std::vector<VkBuffer> shadowUniformBuffers;
    std::vector<VkDeviceMemory> shadowUniformBufferMemories;
    std::vector<void*> shadowUniformMappedMemory;
    std::vector<VkDescriptorSet> shadowDescriptorSets;

    VkSampler shadowSampler;

    VertexResourceManager* vertexResourceManager;

    void createShadowResources();
    void createShadowRenderPass();
    void createShadowFramebuffer();
    void createShadowPipeline();
    void createShadowDescriptorPool();
    void createShadowDescriptorSetLayout();
    void createShadowDescriptorSet();
    void createShadowUniformBuffer();
    void createShadowSampler();
};
