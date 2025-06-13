#include <windows.h>
#include <commdlg.h>
#include "imgui_manager.hpp"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"
#include "vulkan_context.hpp"
#include "swap_chain_manager.hpp"
#include "command_manager.hpp"
#include "vulkan_utils.hpp"
#include <stdexcept>
#include <filesystem>


void ImGuiManager::init(GLFWwindow* window, VulkanContext& vulkanContext, SwapChainManager& swapChainManager, VertexResourceManager& vertexResourceManager, CommandManager& commandManager) {
    // 创建 ImGui 上下文
    this->device = vulkanContext.getDevice();
    this->swapChainManager = &swapChainManager;
    this->vertexResourceManager = &vertexResourceManager;
    this->commandBuffers = commandManager.allocateCommandBuffers(2);

    this->preContentExtent = swapChainManager.getSwapChainExtent();
    this->contentExtent = swapChainManager.getSwapChainExtent();

    createDescriptorPool();
    createRenderPass();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // 启用键盘导航
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // 允许窗口停靠
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // 允许多个 Viewport！
    ImGui::StyleColorsDark();

    // Viewport 会让UI窗口真正弹出来，所以这里给样式做一些小调整（官方推荐）
    // ImGuiStyle& style = ImGui::GetStyle();
    // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    // {
    //     style.WindowRounding = 0.0f;
    //     style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    // }

    // 初始化 ImGui 的 GLFW 后端
    ImGui_ImplGlfw_InitForVulkan(window, true);
    
    // 初始化 ImGui 的 Vulkan 后端
    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = vulkanContext.getInstance();
    initInfo.PhysicalDevice = vulkanContext.getPhysicalDevice();
    initInfo.Device = vulkanContext.getDevice();
    initInfo.QueueFamily = vulkanContext.findQueueFamilies(vulkanContext.getPhysicalDevice()).graphicsFamily.value();
    initInfo.Queue = vulkanContext.getGraphicsQueue();
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = descriptorPool; // 这里可以设置为 VK_NULL_HANDLE，后面会创建一个新的池
    initInfo.MinImageCount = vulkanContext.querySwapChainSupport( vulkanContext.getPhysicalDevice()).capabilities.minImageCount; // 最小图像数量
    initInfo.ImageCount = vulkanContext.querySwapChainSupport( vulkanContext.getPhysicalDevice()).capabilities.minImageCount+1;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.RenderPass = renderPass; 
    initInfo.Allocator = nullptr;
    
    ImGui_ImplVulkan_Init(&initInfo);
    ImGui_ImplVulkan_CreateFontsTexture();
}

void ImGuiManager::cleanup() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);
}

void ImGuiManager::recreatWindow() {
    textureCache.clear();
    textures.clear();
    // ImGui::SetNextWindowSize(ImVec2((float)swapChainManager->getSwapChainExtent().width, (float)swapChainManager->getSwapChainExtent().height));
}

void ImGuiManager::beginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::addTexture(const VkImageView* imageView, VkSampler sampler, VkImageLayout imageLayout) {
    if (textureCache.find(imageView) != textureCache.end()) {
        offScreenTextureId = textureCache[imageView];
        return;
        // return textureCache[imageView];
    }
    // offScreenTextureId = reinterpret_cast<ImTextureID>(ImGui_ImplVulkan_AddTexture(sampler, *imageView, imageLayout));
    ImTextureID textureID = reinterpret_cast<ImTextureID>(ImGui_ImplVulkan_AddTexture(sampler, *imageView, imageLayout));
    textureCache[imageView] = textureID;
    textures.push_back(textureID); // 保存纹理 ID，供后续使用
    offScreenTextureId = textureID;
    // return textureID;
}

VkCommandBuffer ImGuiManager::recordCommandbuffer(uint32_t currentframe, VkFramebuffer framebuffer) {
    VkCommandBuffer commandBuffer = commandBuffers[currentframe];

    // 开始命令缓冲区录制
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // 可选：标记为一次性使用

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer!");
    }
    
    // 开始 ImGui 的 RenderPass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainManager->getSwapChainExtent();

    VkClearValue clearValue{};
    clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // 渲染 ImGui
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    vkCmdEndRenderPass(commandBuffer);

    // 如果启用了 Viewports，更新平台窗口
    // ImGuiIO& io = ImGui::GetIO();
    // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    // {
    //     ImGui::UpdatePlatformWindows();
    //     ImGui::RenderPlatformWindowsDefault();
    // }
    
    // 结束命令缓冲区录制
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer!");
    }
    return commandBuffer;
}

void ImGuiManager::createDescriptorPool()
{
    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool!");
    }
}

void ImGuiManager::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainManager->getSwapChainImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    // dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    // dependency.srcAccessMask = 0;
    // dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    // dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui render pass!");
    }
}

std::string ImGuiManager::openFileDialog() {
    OPENFILENAMEA ofn;       // 结构体，用于配置对话框
    char fileName[MAX_PATH] = ""; // 存储选中的文件路径
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr; // 如果有窗口句柄，可以传入
    ofn.lpstrFilter = "OBJ Files\0*.obj\0All Files\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = "..\\model";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        std::wcout << L"Selected file: " << fileName << std::endl;
        return std::string(fileName); // 返回选中的文件路径
    } else {
        std::wcout << L"No file selected or dialog canceled." << std::endl;
    }
}

VkExtent2D ImGuiManager::renderImGuiInterface(){
    beginFrame();
    
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open", "Ctrl+O")) { 
                std::string objFilePath =  openFileDialog(); // 打开文件对话框
                std::filesystem::path filePath(objFilePath);
                vertexResourceManager->reloadModel(objFilePath, filePath.parent_path().string());
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) { /* Handle save */ }
            if (ImGui::MenuItem("Exit")) { /* Handle exit */ }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    float menuBarHeight = ImGui::GetFrameHeight(); // 通常是菜单栏高度
    ImGui::SetNextWindowPos(ImVec2(0, menuBarHeight)); // 避开菜单栏

    // 设置 ImGui 窗口的位置和大小
    // ImGui::SetNextWindowPos(ImVec2(0, 0)); // 窗口左上角对齐
    ImGui::SetNextWindowSize(ImVec2((float)swapChainManager->getSwapChainExtent().width - 300, (float)swapChainManager->getSwapChainExtent().height - menuBarHeight)); // 窗口大小减去菜单栏高度
    // ImGui::SetNextWindowBgAlpha(0.0f); // 设置窗口背景透明度为 0.0f

    // 渲染 ImGui 界面
    ImGui::Begin("Hello, ImGui!", nullptr, ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoDecoration);
    // 获取窗口可用内容区域（减去标题栏等）
    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    // ImGui::Text("This is a Vulkan ImGui example.");
    ImVec2 imageSize = availableSize;
    // 计算纹理的宽高比
    // ImVec2 textureSize = ImVec2((float)swapChainManager->getSwapChainExtent().width, (float)swapChainManager->getSwapChainExtent().height); // 纹理的原始尺寸
    // float aspectRatio = textureSize.x / textureSize.y;
    // if (availableSize.x / availableSize.y > aspectRatio) {
    //     imageSize.x = availableSize.y * aspectRatio;
    // } else {
    //     imageSize.y = availableSize.x / aspectRatio;
    // }
    // 居中偏移（保留四周 padding）
    ImVec2 cursorPos = ImGui::GetCursorPos();
    ImVec2 offset;
    offset.x = (availableSize.x - imageSize.x) * 0.5f;
    offset.y = (availableSize.y - imageSize.y) * 0.5f;
    ImGui::SetCursorPos(ImVec2(cursorPos.x + offset.x, cursorPos.y + offset.y));
    // 显示纹理
    ImGui::Image(offScreenTextureId, imageSize); // offScreenTextureId 是注册的纹理 ID
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 300, menuBarHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, ImGui::GetIO().DisplaySize.y - menuBarHeight), ImGuiCond_Always);
    ImGui::Begin("Material Editor", nullptr, ImGuiWindowFlags_NoResize);

    std::vector<std::string> shapeNames = vertexResourceManager->getShapeNames();
    auto materialUniformBufferObjects = vertexResourceManager->getMaterialUniformBufferObjects();
    // static std::vector<ImVec4> baseColors(shapeNames.size(), ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    
    for (size_t i = 0; i < shapeNames.size(); ++i) {
        if (ImGui::TreeNode(shapeNames[i].c_str())) {
            auto metallic = &materialUniformBufferObjects[i]->metallic;
            auto roughness = &materialUniformBufferObjects[i]->roughness;
            auto ambientOcclusion = &materialUniformBufferObjects[i]->ambientOcclusion;
            auto baseColors = &materialUniformBufferObjects[i]->albedo;

            ImGui::SliderFloat(("Metallic##" + std::to_string(i)).c_str(), metallic, 0.0f, 1.0f);
            ImGui::SliderFloat(("Roughness##" + std::to_string(i)).c_str(), roughness, 0.0f, 1.0f);
            ImGui::SliderFloat(("ambientOcclusion##" + std::to_string(i)).c_str(), ambientOcclusion, 0.0f, 1.0f);
            ImGui::ColorEdit3(("Base Color##" + std::to_string(i)).c_str(), (float*)baseColors);
            ImGui::TreePop();
        }
    }
    // std::vector<std::string> shapeNames = {"Cube", "Sphere", "Cylinder", "Plane"};
    // static  int currentShapeIndex = 0;
    // if (ImGui::BeginCombo("Shape", shapeNames[currentShapeIndex].c_str())) {
    //     for (int n = 0; n < shapeNames.size(); n++) {
    //         bool is_selected = (currentShapeIndex == n);
    //         if (ImGui::Selectable(shapeNames[n].c_str(), is_selected)) {
    //             currentShapeIndex = n;
    //         }
    //         if (is_selected)
    //             ImGui::SetItemDefaultFocus();
    //     }
    //     ImGui::EndCombo();
    // }
    static float metallic = 0.0f;

    // 可调节金属度（metallic），范围是 0.0 到 1.0
    // ImGui::SliderFloat("Metallic", vertexResourceManager->getMetallics(), 0.0f, 1.0f);
    static float ao = 1.0f;
    // ImGui::SliderFloat("AO", &ao, 0.0f, 1.0f);
    // // 可调节粗糙度（roughness），范围是 0.0 到 1.0
    // ImGui::SliderFloat("Roughness", nullptr, 0.0f, 1.0f);
    
    // // 可选：环境光遮蔽
    // ImGui::SliderFloat("AO", nullptr, 0.0f, 1.0f);
    
    ImGui::End();
    preContentExtent = contentExtent; // 保存上一次的内容区域大小
    contentExtent = {static_cast<uint32_t>(availableSize.x), static_cast<uint32_t>(availableSize.y)};
    return {static_cast<uint32_t>(availableSize.x), static_cast<uint32_t>(availableSize.y)};
}