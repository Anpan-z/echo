#pragma once

#include "gbuffer_resource_manager.hpp"
#include "path_tracing_resource_manager.hpp"
#include "svg_filter_resource_manager.hpp"
#include "swap_chain_manager.hpp"
#include <vector>
#include <vulkan/vulkan.h>

class SVGFilterPassObserver;
class SVGFilterPass
{
  public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice, SwapChainManager& swapChainManager,
              GBufferResourceManager& gbufferResourceManager, PathTracingResourceManager& pathTracingResourceManager,
              SVGFilterResourceManager& svgFilterResourceManager, std::vector<VkCommandBuffer>&& commandBuffers);
    void cleanup();

    VkCommandBuffer recordCommandBuffer(uint32_t frameIndex, uint32_t imageIndex);

    void updatePathTracedColorDescriptorSets();

    void updateGBufferDescriptorSets();

    void updateSVGFilterDescriptorSets();

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

    std::unique_ptr<SVGFilterPassObserver> svgFilterPassObserver;

    void createPipeline();
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();
};

class SVGFilterPassObserver : public SVGFilterImageRecreateObserver,
                              public GBufferResourceRecreateObserver,
                              public PathTracingResourceReloadObserver
{
  public:
    SVGFilterPassObserver(SVGFilterPass* svgFilterPass) : svgFilterPass(svgFilterPass)
    {
    }

    void onSVGFilterImageRecreated() override
    {
        if (svgFilterPass)
        {
            svgFilterPass->updateSVGFilterDescriptorSets();
        }
    }

    void onGBufferRecreated() override
    {
        if (svgFilterPass)
        {
            svgFilterPass->updateGBufferDescriptorSets();
        }
    }

    void onPathTracingOutputImagesRecreated() override
    {
        if (svgFilterPass)
        {
            svgFilterPass->updatePathTracedColorDescriptorSets();
        }
    }

    void onModelReloaded() override
    {
    }

  private:
    SVGFilterPass* svgFilterPass = nullptr;
};