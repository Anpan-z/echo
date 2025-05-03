#pragma once
#include "shadow_mapping.hpp"
#include "swap_chain_manager.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <functional>

class RenderPipelineModelObserver;

class RenderPipeline {
public:
    // 初始化和清理方法
    void init(VkDevice device, VkPhysicalDevice physicalDevice, SwapChainManager& swapChainManager, VertexResourceManager& vertexResourceManager, std::vector<VkCommandBuffer>&& shadowCommandBuffers);
    void cleanup();

    // 初始化渲染管线
    void setup(ShadowMapping& shadowMapping);

    // 录制指令缓冲区
    VkCommandBuffer recordCommandBuffer(uint32_t bufferIndex, VkFramebuffer swapChainFramebuffers);

    // 获取资源
    VkRenderPass getRenderPass() const { return renderPass; }
    VkPipeline getGraphicsPipeline() const { return graphicsPipeline; }
    VkCommandBuffer getCommandBuffer(size_t index) const { return commandBuffers[index]; }
    VkDescriptorPool getDescriptorPool() const { return descriptorPool; } 
    void updateMaterialDescriptorSets();

private:
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    SwapChainManager* swapChainManager = nullptr;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;

    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    VertexResourceManager* vertexResourceManager;

    std::unique_ptr<RenderPipelineModelObserver> pipelineModelReloadObserver;
    // 私有方法
    void createRenderPass();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    // void createDepthResources();
    // void createFramebuffers(const std::vector<VkImageView>& swapChainImageViews);
    // void createCommandPool(uint32_t queueFamilyIndex);
    // void createCommandBuffers(size_t bufferCount);

    void createDescriptorPool(size_t maxFramesInFlight);
    void createDescriptorSets(size_t maxFramesInFlight, ShadowMapping& shadowMapping);
};

class RenderPipelineModelObserver : public ModelReloadObserver {
public:
    RenderPipelineModelObserver(RenderPipeline* renderPipeline) // 使用指向 RenderPipeline 的指针
        : renderPipeline(renderPipeline) {}

    void onModelReloaded() override {
        // 当模型重新加载时，更新 RenderPipeline 的描述符集
        if (renderPipeline) {
            renderPipeline->updateMaterialDescriptorSets();
        }
    }

private:
    RenderPipeline* renderPipeline; // 持有 RenderPipeline 的指针
};