#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GLFW/glfw3.h>

#include "window_manager.hpp"
#include "imgui_manager.hpp"
#include "input_manager.hpp"
#include "shadow_mapping.hpp"
#include "vertex_resource_manager.hpp"
#include "texture_resource_manager.hpp"
#include "render_pipeline.hpp"
#include "vulkan_utils.hpp"
#include "command_manager.hpp"
#include "vulkan_context.hpp"
#include "swap_chain_manager.hpp"
#include "render_target.hpp"
#include "camera.hpp"
#include "IBL_renderer.hpp"
#include "path_tracing_pipeline.hpp"
#include "path_tracing_resource_manager.hpp"

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

// const uint32_t SHADOW_MAP_WIDTH = 2048;
// const uint32_t SHADOW_MAP_HEIGHT = 2048;


//const std::string MODEL_PATH = "CornellBox-Original.obj";
const std::string MODEL_PATH = "../model/CornellBox/CornellBox-Glossy.obj";
//const std::string MTL_PATH = "CornellBox-Original.mtl";
const std::string MTL_PATH = "../model/CornellBox";
const std::string TEXTURE_PATH = "../texture/golden_gate_hills_1k.hdr";

// const int MAX_FRAMES_IN_FLIGHT = 2;
class HelloTriangleApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;

    VkQueue graphicsQueue;
    VkQueue presentQueue;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    
    uint32_t currentFrame = 0;

    WindowManager windowManager;
    InputManager inputManager;
    ImGuiManager imguiManager;
    ShadowMapping shadowMapping;
    RenderPipeline renderPipeline;
    CommandManager commandManager;
    VulkanContext vulkanContext;
    SwapChainManager swapChainManager;
    RenderTarget renderTarget;
    VertexResourceManager vertexResourceManager;
    TextureResourceManager textureResourceManager;

    IBLRenderer iblRenderer;

    PathTracingResourceManager pathTracingResourceManager;
    PathTracingPipeline pathTracingPipeline;

    Camera camera;
    
    void initWindow() {
        windowManager.init(WIDTH, HEIGHT, "Vulkan");

    }

    void initVulkan() {
        vulkanContext.init(windowManager.getWindow());
        device = vulkanContext.getDevice();
        physicalDevice = vulkanContext.getPhysicalDevice();
        graphicsQueue = vulkanContext.getGraphicsQueue();
        presentQueue = vulkanContext.getPresentQueue();

        swapChainManager.init(device, vulkanContext, windowManager.getWindow());

        commandManager.init(device, vulkanContext);
        vertexResourceManager.loadModel(MODEL_PATH, MTL_PATH);
        vertexResourceManager.init(device, physicalDevice, commandManager);

        textureResourceManager.init(device, physicalDevice, graphicsQueue, commandManager);
        textureResourceManager.loadHDRTexture(TEXTURE_PATH);

        pathTracingResourceManager.init(device, physicalDevice, graphicsQueue, swapChainManager, commandManager, vertexResourceManager);
        pathTracingPipeline.init(device, physicalDevice, pathTracingResourceManager, commandManager.allocateCommandBuffers(MAX_FRAMES_IN_FLIGHT));

        iblRenderer.init(device, graphicsQueue, commandManager, textureResourceManager);
        
        // 创建阴影映射和渲染管线
        
        shadowMapping.init(device, physicalDevice, vertexResourceManager, commandManager.allocateCommandBuffers(MAX_FRAMES_IN_FLIGHT));
        renderPipeline.init(device, physicalDevice, swapChainManager, vertexResourceManager, textureResourceManager, commandManager.allocateCommandBuffers(MAX_FRAMES_IN_FLIGHT));
        imguiManager.init(windowManager.getWindow(), vulkanContext, swapChainManager, vertexResourceManager, commandManager);
        renderTarget.init(device, physicalDevice, swapChainManager, renderPipeline.getRenderPass(), imguiManager.getRenderPass());
        
        renderPipeline.setup(shadowMapping);
        
    

        camera.init();
        createSyncObjects();
    }

    float deltaTime = 0.0f; // 每帧的时间间隔
    float lastFrame = 0.0f; // 上一帧的时间
    void mainLoop() {
        while (!windowManager.shouldClose()) {
            windowManager.pollEvents();

            float currentFrame = static_cast<float>(glfwGetTime());
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;
            
            inputManager.processInput(windowManager.getWindow(), camera, deltaTime, WIDTH, HEIGHT);

            drawFrame();
        }
        vkDeviceWaitIdle(device);
    }

    void cleanup() {
        shadowMapping.cleanup();
        vertexResourceManager.cleanup();
        renderPipeline.cleanup();
        commandManager.cleanup();
        swapChainManager.cleanup();
        renderTarget.cleanup();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }
        
        textureResourceManager.cleanup();
        iblRenderer.cleanup();
        pathTracingPipeline.cleanup();
        pathTracingResourceManager.cleanup();

        imguiManager.cleanup();
        vulkanContext.cleanup();
        windowManager.cleanup();
    }


    void createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {

                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }

    }

    void drawFrame() {
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, swapChainManager.getSwapChain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            swapChainManager.recreateSwapChain();
            renderTarget.recreateRenderTarget();
            imguiManager.recreatWindow();
            pathTracingResourceManager.recreatePathTracingOutputImages();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        shadowMapping.updateShadowUniformBuffer(currentFrame); // update lightSpaceMatrix
        vertexResourceManager.updateUniformBuffer(currentFrame, swapChainManager.getSwapChainExtent(), camera);
        pathTracingResourceManager.updateCameraDataBuffer(currentFrame, swapChainManager.getSwapChainExtent(), camera);

        vkResetFences(device, 1, &inFlightFences[currentFrame]);

        vkResetCommandBuffer(renderPipeline.getCommandBuffer(currentFrame), /*VkCommandBufferResetFlagBits*/ 0);
        vkResetCommandBuffer(shadowMapping.getCommandBuffer(currentFrame), /*VkCommandBufferResetFlagBits*/ 0);
        vkResetCommandBuffer(imguiManager.getCommandBuffer(currentFrame), /*VkCommandBufferResetFlagBits*/ 0);
        vkResetCommandBuffer(pathTracingPipeline.getCommandBuffer(currentFrame), /*VkCommandBufferResetFlagBits*/ 0);
        
        imguiManager.addTexture(&pathTracingResourceManager.getPathTracingOutputImageviews()[imageIndex], renderTarget.getOffScreenSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        // imguiManager.addTexture(&renderTarget.getOffScreenImageView()[imageIndex], renderTarget.getOffScreenSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkExtent2D contentSize = imguiManager.renderImGuiInterface();
        
        auto shadowcommandBuffer = shadowMapping.recordShadowCommandBuffer(currentFrame);
        auto commandBuffer =  renderPipeline.recordCommandBuffer(currentFrame, renderTarget.getOffScreenFramebuffers()[imageIndex]);
        auto pathTracingCommandBuffer = pathTracingPipeline.recordCommandBuffer(currentFrame, imageIndex);

        VkCommandBuffer imguiCommandBuffer =  imguiManager.recordCommandbuffer(currentFrame, renderTarget.getFramebuffers()[imageIndex]);
        
        std::array<VkCommandBuffer, 4>commandBuffers = {shadowcommandBuffer, pathTracingCommandBuffer, commandBuffer, imguiCommandBuffer};

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
        submitInfo.pCommandBuffers = commandBuffers.data();

        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { swapChainManager.getSwapChain() };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(presentQueue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || windowManager.isFramebufferResized()) {
            windowManager.setFramebufferResized(false);
            swapChainManager.recreateSwapChain();
            renderTarget.recreateRenderTarget();
            imguiManager.recreatWindow();
            pathTracingResourceManager.recreatePathTracingOutputImages();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

};



int main() {
    HelloTriangleApplication app;

    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}