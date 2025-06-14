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
#include "gbuffer_pass.hpp"
#include "svg_filter_pass.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h> // 用于保存 PNG 图像
#include <stb_image_resize2.h> // 用于调整图像大小
#include <algorithm>

// uint32_t WIDTH = 800;
// uint32_t HEIGHT = 600;

// const uint32_t SHADOW_MAP_WIDTH = 2048;
// const uint32_t SHADOW_MAP_HEIGHT = 2048;


//const std::string MODEL_PATH = "CornellBox-Original.obj";
// std::string MODEL_PATH = "../model/CornellBox/CornellBox-Original.obj";
//const std::string MTL_PATH = "CornellBox-Original.mtl";
// std::string MTL_PATH = "../model/CornellBox";
// std::string TEXTURE_PATH = "../texture/golden_gate_hills_1k.hdr";

// const int MAX_FRAMES_IN_FLIGHT = 2;
class ECHOOFFLINE {
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

    GBufferPass gbufferPass;
    GBufferResourceManager gbufferResourceManager;
    SVGFilterPass svgFilterPass;
    SVGFilterResourceManager svgFilterResourceManager;

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
        
        gbufferPass.init(device, physicalDevice, swapChainManager, vertexResourceManager, commandManager.allocateCommandBuffers(MAX_FRAMES_IN_FLIGHT));
        gbufferResourceManager.init(device, physicalDevice, gbufferPass.getGBufferRenderPass(), swapChainManager);
        svgFilterResourceManager.init(device, physicalDevice, graphicsQueue, swapChainManager, commandManager);
        svgFilterPass.init(device, physicalDevice, swapChainManager, gbufferResourceManager, pathTracingResourceManager, svgFilterResourceManager, commandManager.allocateCommandBuffers(MAX_FRAMES_IN_FLIGHT));
        camera.init();
        createSyncObjects();
    }

    float deltaTime = 0.0f; // 每帧的时间间隔
    float lastFrame = 0.0f; // 上一帧的时间
    void mainLoop() {
        int frameCount = 2560;
        while (!windowManager.shouldClose()) {
            windowManager.pollEvents();

            float currentFrame = static_cast<float>(glfwGetTime());
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;
            
            inputManager.processInput(windowManager.getWindow(), camera, deltaTime, WIDTH, HEIGHT);

            drawFrame();
            frameCount--;
            if (frameCount == 0) {
                // 保存当前帧的图像到文件
                VkImage currentImage = pathTracingResourceManager.getPathTracingOutputImages()[0];
                uint32_t imageSize = imguiManager.getContentExtent().width * imguiManager.getContentExtent().height * 4 * sizeof(float); // 4 channels, float
                saveImageToFile(currentImage, imageSize);
            }
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
        gbufferPass.cleanup();
        gbufferResourceManager.cleanup();
        svgFilterPass.cleanup();
        svgFilterResourceManager.cleanup();

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

    //计算最大外接矩形，返回归一化坐标
    void detect_content_bbox_normalized(
        const uint8_t* pixels, int width, int height, int channels,
        double& s0, double& t0, double& s1, double& t1)
    {
        int min_x = width, max_x = -1, min_y = height, max_y = -1;
    
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const uint8_t* p = pixels + (y * width + x) * channels;
                // 判断是否为非黑色像素
                if (p[0] != 0 || p[1] != 0 || p[2] != 0) {
                    min_x = std::min(min_x, x);
                    max_x = std::max(max_x, x);
                    min_y = std::min(min_y, y);
                    max_y = std::max(max_y, y);
                }
            }
        }
    
        // 全黑图片时，返回全图
        if (max_x < min_x || max_y < min_y) {
            s0 = 0.0;
            t0 = 0.0;
            s1 = 1.0;
            t1 = 1.0;
            return;
        }
    
        // 输出归一化坐标
        s0 = static_cast<double>(min_x) / width;
        t0 = static_cast<double>(min_y) / height;
        s1 = static_cast<double>(max_x + 1) / width;
        t1 = static_cast<double>(max_y + 1) / height;
    }

    // 计算最大全非黑色内接矩形，返回归一化坐标
    void detect_largest_full_nonblack_rect_normalized(
        const uint8_t* pixels, int width, int height, int channels,
        double& s0, double& t0, double& s1, double& t1)
    {
        // 1. 生成二值掩码（非黑色为1，黑色为0）
        std::vector<std::vector<int>> mask(height, std::vector<int>(width));
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const uint8_t* p = pixels + (y * width + x) * channels;
                mask[y][x] = (p[0] != 0 || p[1] != 0 || p[2] != 0) ? 1 : 0;
            }
        }

        // 2. 动态规划：最大全1子矩形
        int max_area = 0, best_left = 0, best_right = 0, best_top = 0, best_bottom = 0;
        std::vector<int> height_arr(width, 0);
        for (int row = 0; row < height; ++row) {
            // 更新每列“连续非黑像素高度”
            for (int col = 0; col < width; ++col)
                height_arr[col] = mask[row][col] ? height_arr[col] + 1 : 0;

            // 单调栈求当前行的最大矩形
            std::vector<int> stack;
            int col = 0;
            while (col <= width) {
                int h = (col == width ? 0 : height_arr[col]);
                if (stack.empty() || h >= height_arr[stack.back()]) {
                    stack.push_back(col++);
                } else {
                    int top = stack.back();
                    stack.pop_back();
                    int w = stack.empty() ? col : (col - stack.back() - 1);
                    int area = height_arr[top] * w;
                    if (area > max_area) {
                        max_area = area;
                        best_left = (stack.empty() ? 0 : stack.back() + 1);
                        best_right = col - 1;
                        best_bottom = row;
                        best_top = row - height_arr[top] + 1;
                    }
                }
            }
        }

        if (max_area == 0) {
            // 没有非黑色子矩形，返回全图
            s0 = 0.0; t0 = 0.0; s1 = 1.0; t1 = 1.0;
            return;
        }

        // 归一化坐标（注意右下角要+1）
        s0 = static_cast<double>(best_left) / width;
        t0 = static_cast<double>(best_top) / height;
        s1 = static_cast<double>(best_right + 1) / width;
        t1 = static_cast<double>(best_bottom + 1) / height;
    }

    // 归一化坐标s0,t0,s1,t1范围[0,1]，分别为左上、右下角
    // 返回裁剪后的新buffer，输出宽高通过参数返回
    // pixels: 原图数据，width/height: 原图尺寸，channels: 通道数
    std::vector<uint8_t> crop_image(
        const uint8_t* pixels, int width, int height, int channels,
        double s0, double t0, double s1, double t1,
        int& out_width, int& out_height)
    {
        // 转为像素坐标，注意clamp边界
        int x0 = std::max(0, std::min(width - 1, static_cast<int>(s0 * width + 0.5)));
        int x1 = std::max(0, std::min(width,     static_cast<int>(s1 * width + 0.5)));
        int y0 = std::max(0, std::min(height - 1, static_cast<int>(t0 * height + 0.5)));
        int y1 = std::max(0, std::min(height,     static_cast<int>(t1 * height + 0.5)));

        out_width = x1 - x0;
        out_height = y1 - y0;
        if (out_width <= 0 || out_height <= 0) {
            // 无内容，返回空
            return {};
        }

        std::vector<uint8_t> out(out_width * out_height * channels);

        for (int y = 0; y < out_height; ++y) {
            const uint8_t* src = pixels + ((y0 + y) * width + x0) * channels;
            uint8_t* dst = out.data() + y * out_width * channels;
            std::copy(src, src + out_width * channels, dst);
        }
        return out;
    }

    struct vec3_simple { // Using a simple struct to avoid full GLM dependency if not already used here
        float r, g, b;
        vec3_simple(float r_ = 0.f, float g_ = 0.f, float b_ = 0.f) : r(r_), g(g_), b(b_) {}
        vec3_simple operator+(const vec3_simple& other) const {
            return vec3_simple(r + other.r, g + other.g, b + other.b);
        }
        vec3_simple operator/(const vec3_simple& other) const {
            return vec3_simple(r / other.r, g / other.g, b / other.b);
        }
        // Division by scalar
        vec3_simple operator/(float scalar) const {
            return vec3_simple(r / scalar, g / scalar, b / scalar);
        }
    };

    vec3_simple reinhardTonemap(vec3_simple hdrColor) {
        hdrColor = hdrColor / (hdrColor + vec3_simple(1.0f, 1.0f, 1.0f));
        return hdrColor;
    }

    void saveImageToFile(VkImage sourceImage, uint32_t imageSize) {
        VulkanUtils& vulkanUtils = VulkanUtils::getInstance();

        uint32_t width = imguiManager.getContentExtent().width;
        uint32_t height = imguiManager.getContentExtent().height;

        // Ensure imageSize calculation is correct for VK_FORMAT_R32G32B32A32_SFLOAT
        // imageSize = width * height * 4 * sizeof(float); // This was passed as parameter, ensure it's correct

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        vulkanUtils.createBuffer(device, physicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
        
        VkCommandPool commandPool = commandManager.getCommandPool();
        // Transition image layout for transfer
        vulkanUtils.transitionImageLayout(device, commandPool, graphicsQueue, sourceImage, VK_FORMAT_R32G32B32A32_SFLOAT , VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        
        VkCommandBuffer commandBuffer = vulkanUtils.beginSingleTimeCommands(device, commandPool);
        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};
        vkCmdCopyImageToBuffer(commandBuffer, sourceImage,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &region);
        vulkanUtils.endSingleTimeCommands(device, commandPool, graphicsQueue, commandBuffer);

        // Transition image layout back
        vulkanUtils.transitionImageLayout(device, commandPool, graphicsQueue, sourceImage, VK_FORMAT_R32G32B32A32_SFLOAT , VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        
        void* mappedData;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &mappedData);
        
        float* hdrData = static_cast<float*>(mappedData); // Correctly cast to float*
        uint8_t* ldrProcessedData = new uint8_t[width * height * 4]; // Buffer for final LDR RGBA data
        
        auto toSRGB = [](float linear) -> float {
            if (linear <= 0.0031308f)
                return linear * 12.92f;
            else
                return 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
        };
        
        for (uint32_t i = 0; i < width * height; ++i) {
            // Read HDR float values
            vec3_simple hdrColor(hdrData[i * 4 + 0], hdrData[i * 4 + 1], hdrData[i * 4 + 2]);
            float hdrAlpha = hdrData[i * 4 + 3];

            // 1. Tonemap RGB (Alpha is usually handled differently or passed through)
            vec3_simple ldrLinearColor = reinhardTonemap(hdrColor);
        
            // 2. Gamma Correct RGB
            float r_srgb = toSRGB(ldrLinearColor.r);
            float g_srgb = toSRGB(ldrLinearColor.g);
            float b_srgb = toSRGB(ldrLinearColor.b);
        
            // 3. Quantize to uint8_t
            ldrProcessedData[i * 4 + 0] = static_cast<uint8_t>(std::clamp(r_srgb, 0.0f, 1.0f) * 255.0f);
            ldrProcessedData[i * 4 + 1] = static_cast<uint8_t>(std::clamp(g_srgb, 0.0f, 1.0f) * 255.0f);
            ldrProcessedData[i * 4 + 2] = static_cast<uint8_t>(std::clamp(b_srgb, 0.0f, 1.0f) * 255.0f);
            
            // Handle Alpha: typically clamp and scale if it's also HDR, or set to opaque
            // Assuming alpha from SFLOAT is also a float that needs clamping and scaling.
            ldrProcessedData[i * 4 + 3] = static_cast<uint8_t>(std::clamp(hdrAlpha, 0.0f, 1.0f) * 255.0f);
            // If you know your path tracer always outputs opaque (alpha=1.0), you can just use:
            // ldrProcessedData[i * 4 + 3] = 255;
        }

        double s0, t0, s1, t1;
        detect_largest_full_nonblack_rect_normalized(ldrProcessedData, width, height, 4, s0, t0, s1, t1);
        // detect_content_bbox_normalized(ldrProcessedData, width, height, 4, s0, t0, s1, t1);

        int crop_w, crop_h;
        std::vector<uint8_t> cropped = crop_image(ldrProcessedData, width, height, 4, s0, t0, s1, t1, crop_w, crop_h);
        

        // Save the processed LDR data as PNG
        stbi_write_png("output_sfloat_spp_1_test.png", crop_w, crop_h, 4, cropped.data(), crop_w * 4);
        
        delete[] ldrProcessedData; // Don't forget to free allocated memory

        vkUnmapMemory(device, stagingBufferMemory);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
        std::cout << "Image saved to output_sfloat_processed.png" << std::endl;
    }


    void drawFrame() {
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, swapChainManager.getSwapChain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            swapChainManager.recreateSwapChain();
            renderTarget.recreateRenderTarget(imguiManager.getContentExtent());
            imguiManager.recreatWindow();
            pathTracingResourceManager.recreatePathTracingOutputImages(imguiManager.getContentExtent());
            gbufferResourceManager.recreateGBuffer(imguiManager.getContentExtent());
            svgFilterResourceManager.recreateDenoisedOutputImages(imguiManager.getContentExtent());
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        imguiManager.addTexture(&svgFilterResourceManager.getDenoisedOutputImageView()[imageIndex], renderTarget.getOffScreenSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        // imguiManager.addTexture(&pathTracingResourceManager.getPathTracingOutputImageviews()[imageIndex], renderTarget.getOffScreenSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        // imguiManager.addTexture(&renderTarget.getOffScreenImageView()[imageIndex], renderTarget.getOffScreenSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkExtent2D contentSize = imguiManager.renderImGuiInterface();

        shadowMapping.updateShadowUniformBuffer(currentFrame); // update lightSpaceMatrix
        vertexResourceManager.updateUniformBuffer(currentFrame, contentSize, camera);
        pathTracingResourceManager.updateCameraDataBuffer(currentFrame, contentSize, camera);

        vkResetFences(device, 1, &inFlightFences[currentFrame]);

        vkResetCommandBuffer(renderPipeline.getCommandBuffer(currentFrame), /*VkCommandBufferResetFlagBits*/ 0);
        vkResetCommandBuffer(shadowMapping.getCommandBuffer(currentFrame), /*VkCommandBufferResetFlagBits*/ 0);
        vkResetCommandBuffer(imguiManager.getCommandBuffer(currentFrame), /*VkCommandBufferResetFlagBits*/ 0);
        vkResetCommandBuffer(pathTracingPipeline.getCommandBuffer(currentFrame), /*VkCommandBufferResetFlagBits*/ 0);
        
        
        auto shadowcommandBuffer = shadowMapping.recordShadowCommandBuffer(currentFrame);
        auto commandBuffer =  renderPipeline.recordCommandBuffer(currentFrame, renderTarget.getOffScreenFramebuffers()[imageIndex], renderTarget.getOffScreenExtent());
        auto pathTracingCommandBuffer = pathTracingPipeline.recordCommandBuffer(currentFrame, imageIndex);
        auto gbufferCommandBuffer = gbufferPass.recordCommandBuffer(currentFrame, gbufferResourceManager.getFramebuffer(imageIndex), gbufferResourceManager.getOutputExtent());
        auto svgFilterCommandBuffer = svgFilterPass.recordCommandBuffer(currentFrame, imageIndex);

        VkCommandBuffer imguiCommandBuffer =  imguiManager.recordCommandbuffer(currentFrame, renderTarget.getFramebuffers()[imageIndex]);
        
        std::array<VkCommandBuffer, 6> commandBuffers = {shadowcommandBuffer, pathTracingCommandBuffer, commandBuffer, gbufferCommandBuffer, svgFilterCommandBuffer, imguiCommandBuffer};

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

        if (imguiManager.getContentExtent().width != imguiManager.getPreContentExtent().width ||
            imguiManager.getContentExtent().height != imguiManager.getPreContentExtent().height){
                //延迟一帧更新，等待imgui计算出新的内容大小
                vkDeviceWaitIdle(device);
                renderTarget.recreateOffScreenTarget(contentSize);
                imguiManager.recreatWindow();
                pathTracingResourceManager.recreatePathTracingOutputImages(contentSize);
                gbufferResourceManager.recreateGBuffer(contentSize);
                svgFilterResourceManager.recreateDenoisedOutputImages(contentSize);
            }
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || windowManager.isFramebufferResized()) {
            windowManager.setFramebufferResized(false);
            swapChainManager.recreateSwapChain();
            renderTarget.recreatePresentTarget();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

};