#pragma once

#include "path_tracing_resource_manager.hpp"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

class PathTracingPipelineObserver;

class PathTracingPipeline {
public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice, PathTracingResourceManager& pathTracingResourceManager, std::vector<VkCommandBuffer>&& commandBuffers);
    void cleanup();

    VkCommandBuffer recordCommandBuffer(uint32_t frameIndex, uint32_t imageIndex);

    void recreateOutputImageResource();

    void updateStorageBufferDescriptorSet();

    VkCommandBuffer getCommandBuffer(size_t index) const { return pathTracingCommandBuffers[index]; }
private:
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    PathTracingResourceManager* pathTracingResourceManager = nullptr;
    std::vector<VkCommandBuffer> pathTracingCommandBuffers;

    VkPipeline pathTracingPipeline = VK_NULL_HANDLE;
    VkPipelineLayout pathTracingPipelineLayout = VK_NULL_HANDLE;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    // 与 Swapchain 图像数量一致的 Descriptor Sets 和 Layouts
    std::vector<VkDescriptorSet> imageDescriptorSets;
    VkDescriptorSetLayout imageDescriptorSetLayout;

    // 与 MAX_FRAMES_IN_FLIGHT 一致的 Descriptor Sets 和 Layouts
    std::vector<VkDescriptorSet> frameDescriptorSets;
    VkDescriptorSetLayout frameDescriptorSetLayout;

    std::unique_ptr<PathTracingPipelineObserver> pathTracingPipelineObserver;

    void createPathTracingPipeline();
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();
};

class PathTracingPipelineObserver : public PathTracingResourceReloadObserver {
    public:
        PathTracingPipelineObserver(PathTracingPipeline* pathTracingPipeline) // 使用指向 RenderPipeline 的指针
            : pathTracingPipeline(pathTracingPipeline) {}
    
        void onModelReloaded() override {
            // 当模型重新加载时，更新 RenderPipeline 的描述符集
            if (pathTracingPipeline) {
                pathTracingPipeline->updateStorageBufferDescriptorSet();
            }
        }

        void onPathTracingOutputImagesRecreated() override {
            // 当路径追踪输出图像重新创建时，更新 RenderPipeline 的描述符集
            if (pathTracingPipeline) {
                pathTracingPipeline->recreateOutputImageResource();
            }
        }
    
    private:
        PathTracingPipeline* pathTracingPipeline; // 持有 RenderPipeline 的指针
};