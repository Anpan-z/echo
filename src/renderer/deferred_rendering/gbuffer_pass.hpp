#pragma once

#include "gbuffer_resource_manager.hpp"
#include "vertex_resource_manager.hpp"
#include <vulkan/vulkan.h>

class GBufferPass
{
  public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice, SwapChainManager& swapChainManager,
              VertexResourceManager& vertexResourceManager, std::vector<VkCommandBuffer>&& CommandBuffers);

    void cleanup();

    VkCommandBuffer recordCommandBuffer(uint32_t frameIndex);

  private:
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    VkRenderPass gbufferRenderPass = VK_NULL_HANDLE;
    VkPipeline gbufferPipeline = VK_NULL_HANDLE;
    VkPipelineLayout gbufferPipelineLayout = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> gbufferCommandBuffers;

    VkDescriptorPool gbufferDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout gbufferDescriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> gbufferDescriptorSets;

    uint32_t width;
    uint32_t height;

    GBufferResourceManager gbufferResourceManager;
    VertexResourceManager* vertexResourceManager;

    void createRenderPass();
    void createDescriptorSetLayout();
    void createPipeline();
    void createDescriptorPool(uint32_t maxFramesInFlight);
    void createDescriptorSets(uint32_t maxFramesInFlight);
};