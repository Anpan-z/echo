#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "vulkan_context.hpp"

class CommandManager {
public:
    void init(VkDevice device, VulkanContext& vulkanContext);
    void cleanup();

    std::vector<VkCommandBuffer> allocateCommandBuffers(size_t maxFramesInFlight);
    
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    // VkCommandBuffer getCommandBuffer(size_t frameIndex) const { return commandBuffers[frameIndex]; }
    VkCommandPool getCommandPool() const { return commandPool; }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    // std::vector<VkCommandBuffer> commandBuffers;
    VulkanContext* vulkanContext = nullptr;

    void createCommandPool();
};