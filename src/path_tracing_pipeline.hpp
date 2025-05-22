#pragma once

#include "path_tracing_resource_manager.hpp"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

class PathTracingPipeline {
public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice, PathTracingResourceManager& pathTracingResourceManager, std::vector<VkCommandBuffer>&& commandBuffers);
    void cleanup();

    VkCommandBuffer recordCommandBuffer(uint32_t frameIndex, uint32_t imageIndex);

    void recreateOutputImageResource();

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

    void createPathTracingPipeline();
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();
};