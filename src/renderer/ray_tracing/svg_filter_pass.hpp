#pragma once

#include "gbuffer_resource_manager.hpp"
#include "path_tracing_resource_manager.hpp"
#include "svg_filter_resource_manager.hpp"
#include "swap_chain_manager.hpp"
#include <vector>
#include <vulkan/vulkan.h>

class SVGFilterPass
{
  public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice, SwapChainManager& swapChainManager,
              GBufferResourceManager& gbufferResourceManager, PathTracingResourceManager& pathTracingResourceManager,
              SVGFilterResourceManager& svgFilterResourceManager, std::vector<VkCommandBuffer>&& commandBuffers);
    void cleanup();

    VkCommandBuffer recordCommandBuffer(uint32_t frameIndex, uint32_t imageIndex);

  private:
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    SwapChainManager* swapChainManager = nullptr;

    GBufferResourceManager* gbufferResourceManager = nullptr;
    PathTracingResourceManager* pathTracingResourceManager = nullptr;
    SVGFilterResourceManager* svgFilterResourceManager = nullptr;
    uint32_t imageCount;

    std::vector<VkCommandBuffer> commandBuffers;

    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    void createPipeline();
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();
};