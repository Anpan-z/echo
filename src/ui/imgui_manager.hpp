#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <unordered_map>
#ifdef IMGUI_IMPL_VULKAN_NO_PROTOTYPES
#undef IMGUI_IMPL_VULKAN_NO_PROTOTYPES
#endif
#include "imgui.h"
#include "vulkan_context.hpp"
#include "swap_chain_manager.hpp"
#include "command_manager.hpp"
#include "vertex_resource_manager.hpp"

class ImGuiManager {
public:
    // 初始化 ImGui
    void init(GLFWwindow* window, VulkanContext& vulkanContext, SwapChainManager& swapChainManager, VertexResourceManager& vertexResourceManager, CommandManager& commandManager);

    // 开始 ImGui 帧
    void beginFrame();

    // 渲染 ImGui
    VkExtent2D renderImGuiInterface();

    // 清理 ImGui 资源
    void cleanup();

    VkRenderPass getRenderPass() const { return renderPass; }

    VkExtent2D getContentExtent() const { return contentExtent; }

    VkExtent2D getPreContentExtent() const { return preContentExtent; }

    void addTexture(const VkImageView* imageView, VkSampler sampler, VkImageLayout imageLayout);
    VkCommandBuffer getCommandBuffer(uint32_t currentFrame) { return commandBuffers[currentFrame]; }
    VkCommandBuffer recordCommandbuffer(uint32_t currentFrame, VkFramebuffer Framebuffer);
    void recreatWindow();
private:
    SwapChainManager* swapChainManager = nullptr;
    VertexResourceManager* vertexResourceManager = nullptr;
    VkDevice device = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;

    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<ImTextureID> textures; // 存储纹理 ID
    std::unordered_map<const VkImageView*, ImTextureID> textureCache;

    ImTextureID offScreenTextureId;

    VkExtent2D preContentExtent;
    VkExtent2D contentExtent;
    void createDescriptorPool();
    void createRenderPass();
    std::string openFileDialog();
};