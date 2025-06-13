#pragma once

#include "gbuffer_resource_manager.hpp"
#include "vertex_resource_manager.hpp"
#include <vulkan/vulkan.h>

class GBufferPassObserver;

class GBufferPass
{
  public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice, SwapChainManager& swapChainManager,
              VertexResourceManager& vertexResourceManager, std::vector<VkCommandBuffer>&& CommandBuffers);

    void cleanup();

    VkCommandBuffer recordCommandBuffer(uint32_t frameIndex, VkFramebuffer framebuffer, VkExtent2D imageExtent);

    VkRenderPass getGBufferRenderPass() const
    {
        return gbufferRenderPass;
    }

    void updateUBODescriptorSets();

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

    VertexResourceManager* vertexResourceManager = nullptr;
    SwapChainManager* swapChainManager = nullptr;

    std::unique_ptr<GBufferPassObserver> gbufferPassObserver;

    void createRenderPass();
    void createDescriptorSetLayout();
    void createPipeline();
    void createDescriptorPool(uint32_t maxFramesInFlight);
    void createDescriptorSets(uint32_t maxFramesInFlight);
};

class GBufferPassObserver : public ModelReloadObserver
{
  public:
    GBufferPassObserver(GBufferPass* gBufferPass) // 使用指向 RenderPipeline 的指针
        : gBufferPass(gBufferPass)
    {
    }

    void onModelReloaded() override
    {
        // 当模型重新加载时，更新 RenderPipeline 的描述符集
        if (gBufferPass)
        {
            gBufferPass->updateUBODescriptorSets();
        }
    }

  private:
    GBufferPass* gBufferPass; // 持有 RenderPipeline 的指针
};