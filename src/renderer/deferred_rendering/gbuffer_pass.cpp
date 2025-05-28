#include "gbuffer_pass.hpp"
#include "vertex.hpp"
#include "vulkan_utils.hpp"
#include <array>
#include <shaderc/shaderc.hpp>
#include <span>
#include <stdexcept>

void GBufferPass::init(VkDevice device, VkPhysicalDevice physicalDevice, SwapChainManager& swapChainManager,
                       VertexResourceManager& vertexResourceManager, std::vector<VkCommandBuffer>&& CommandBuffers)
{
    this->device = device;
    this->physicalDevice = physicalDevice;
    this->width = swapChainManager.getSwapChainExtent().width;
    this->height = swapChainManager.getSwapChainExtent().height;
    this->vertexResourceManager = &vertexResourceManager;
    this->gbufferCommandBuffers = std::move(CommandBuffers);

    createRenderPass();
    gbufferResourceManager.init(device, physicalDevice, gbufferRenderPass, swapChainManager);
    createDescriptorSetLayout();
    createPipeline();
    createDescriptorPool(MAX_FRAMES_IN_FLIGHT);
    createDescriptorSets(MAX_FRAMES_IN_FLIGHT);
}

void GBufferPass::cleanup()
{
    if (gbufferPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, gbufferPipeline, nullptr);
        gbufferPipeline = VK_NULL_HANDLE;
    }

    if (gbufferPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, gbufferPipelineLayout, nullptr);
        gbufferPipelineLayout = VK_NULL_HANDLE;
    }

    if (gbufferRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(device, gbufferRenderPass, nullptr);
        gbufferRenderPass = VK_NULL_HANDLE;
    }

    if (gbufferDescriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, gbufferDescriptorSetLayout, nullptr);
    }

    if (gbufferDescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device, gbufferDescriptorPool, nullptr);
        gbufferDescriptorPool = VK_NULL_HANDLE;
    }

    gbufferResourceManager.cleanup();
}

VkCommandBuffer GBufferPass::recordCommandBuffer(uint32_t frameIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkCommandBuffer commandBuffer = gbufferCommandBuffers[frameIndex];
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    // 开始渲染通道
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = gbufferRenderPass;                                  // 使用 GBuffer 的 Render Pass
    renderPassInfo.framebuffer = gbufferResourceManager.getFramebuffer(frameIndex); // 获取当前帧的 Framebuffer
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {width, height};

    // 清除值（颜色和深度）
    std::vector<VkClearValue> clearValues(4);
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // 清除颜色附件
    clearValues[1].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // 清除法线附件
    clearValues[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // 清除位置附件
    clearValues[3].depthStencil = {1.0f, 0};           // 清除深度附件

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // 绑定渲染管线
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gbufferPipeline);

    // 设置视口
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(width);
    viewport.height = static_cast<float>(height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    // 设置裁剪区域
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {width, height};
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // 绑定顶点和索引缓冲
    VkBuffer vertexBuffer = vertexResourceManager->getVertexBuffer();
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);

    VkBuffer indexBuffer = vertexResourceManager->getIndexBuffer();
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gbufferPipelineLayout, 0, 1,
                            &gbufferDescriptorSets[frameIndex], 0, nullptr);

    // 绘制场景
    uint32_t indexCount = vertexResourceManager->getIndices().size();
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

    // 结束 Render Pass
    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to record command buffer!");
    }
    return commandBuffer;
}

void GBufferPass::createRenderPass()
{
    // 定义颜色附件（漫反射颜色）
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = VK_FORMAT_B8G8R8A8_SRGB; // 使用 sRGB 格式
    // colorAttachment.format = VK_FORMAT_R8G8B8A8_SRGB; // 使用 sRGB 格式
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // 清除操作
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // 存储操作
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 定义法线附件
    VkAttachmentDescription normalAttachment = {};
    normalAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT; // 高精度法线
    normalAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    normalAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    normalAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    normalAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    normalAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    normalAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    normalAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 定义位置附件
    VkAttachmentDescription positionAttachment = {};
    positionAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT; // 高精度位置
    positionAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    positionAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    positionAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    positionAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    positionAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    positionAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    positionAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 定义深度附件
    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT; // 深度格式
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // 不需要存储深度
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // 定义子通道引用
    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0; // 第一个附件
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference normalAttachmentRef = {};
    normalAttachmentRef.attachment = 1; // 第二个附件
    normalAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference positionAttachmentRef = {};
    positionAttachmentRef.attachment = 2; // 第三个附件
    positionAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 3; // 第四个附件
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // 定义子通道
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 3; // 三个颜色附件
    VkAttachmentReference colorAttachments[] = {colorAttachmentRef, normalAttachmentRef, positionAttachmentRef};
    subpass.pColorAttachments = colorAttachments;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // 定义子通道依赖
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // 定义 Render Pass 信息
    std::array<VkAttachmentDescription, 4> attachments = {colorAttachment, normalAttachment, positionAttachment,
                                                          depthAttachment};
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    // 创建 Render Pass
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &gbufferRenderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create G-Buffer render pass!");
    }
}

void GBufferPass::createPipeline()
{
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();

    std::string vertex_shader_code_path = "../shader/gbuffer.vert";
    std::string fragment_shader_code_path = "../shader/gbuffer.frag";

    std::string vs = vulkanUtils.readFileToString(vertex_shader_code_path);
    shaderc::Compiler compiler;
    // 编译顶点着色器，参数分别是着色器代码字符串，着色器类型，文件名
    auto vertResult = compiler.CompileGlslToSpv(vs, shaderc_glsl_vertex_shader, vertex_shader_code_path.c_str());
    auto errorInfo_vert = vertResult.GetErrorMessage();
    if (!errorInfo_vert.empty())
    {
        throw std::runtime_error("Vertex shader compilation error: " + errorInfo_vert);
    }
    // 可以加判断，如果有错误信息(errorInfo!=""),就抛出异常

    std::span<const uint32_t> vert_spv = {vertResult.begin(), size_t(vertResult.end() - vertResult.begin()) * 4};
    VkShaderModuleCreateInfo vsmoduleCreateInfo; // 准备顶点着色器模块创建信息
    vsmoduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vsmoduleCreateInfo.pNext = nullptr; // 自定义数据的指针
    vsmoduleCreateInfo.flags = 0;
    vsmoduleCreateInfo.codeSize = vert_spv.size(); // 顶点着色器SPV数据总字节数
    vsmoduleCreateInfo.pCode = vert_spv.data();    // 顶点着色器SPV数据

    std::string fs = vulkanUtils.readFileToString(fragment_shader_code_path);
    auto fragResult = compiler.CompileGlslToSpv(fs, shaderc_glsl_fragment_shader, fragment_shader_code_path.c_str());
    auto errorInfo_frag = fragResult.GetErrorMessage();
    if (!errorInfo_frag.empty())
    {
        throw std::runtime_error("Fragment shader compilation error: " + errorInfo_frag);
    }
    // 可以加判断，如果有错误信息(errorInfo!=""),就抛出异常
    std::span<const uint32_t> frag_spv = {fragResult.begin(), size_t(fragResult.end() - fragResult.begin()) * 4};
    VkShaderModuleCreateInfo fsmoduleCreateInfo; // 准备顶点着色器模块创建信息
    fsmoduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fsmoduleCreateInfo.pNext = nullptr; // 自定义数据的指针
    fsmoduleCreateInfo.flags = 0;
    fsmoduleCreateInfo.codeSize = frag_spv.size(); // 顶点着色器SPV数据总字节数
    fsmoduleCreateInfo.pCode = frag_spv.data();    // 顶点着色器SPV数据

    // 加载着色器模块
    auto vertShaderModule = vulkanUtils.createShaderModule(device, vsmoduleCreateInfo);
    auto fragShaderModule = vulkanUtils.createShaderModule(device, fsmoduleCreateInfo);

    // 顶点着色器阶段
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    // 片段着色器阶段
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // 顶点输入
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // 输入装配
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // 视口和剪裁
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(width);
    viewport.height = static_cast<float>(height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {width, height};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // 光栅化
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // 多重采样
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // 深度和模板测试
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // 颜色混合
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments(3);
    for (auto& attachment : colorBlendAttachments)
    {
        attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        attachment.blendEnable = VK_FALSE;
    }

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
    colorBlending.pAttachments = colorBlendAttachments.data();
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    // 动态状态
    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // 管线布局
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1; // 使用一个描述符集布局
    pipelineLayoutInfo.pSetLayouts = &gbufferDescriptorSetLayout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &gbufferPipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    // 图形管线创建信息
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = gbufferPipelineLayout;
    pipelineInfo.renderPass = gbufferRenderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &gbufferPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }

    // 清理着色器模块
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
}

void GBufferPass::createDescriptorSetLayout()
{
    // 创建 Uniform Buffer Object 的描述符布局
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo uboLayoutInfo{};
    uboLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    uboLayoutInfo.bindingCount = 1;
    uboLayoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &uboLayoutInfo, nullptr, &gbufferDescriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create UBO descriptor set layout!");
    }
}

void GBufferPass::createDescriptorPool(uint32_t frameCount)
{
    std::array<VkDescriptorPoolSize, 1> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(frameCount); // 每帧一个 UBO

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = frameCount;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &gbufferDescriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
}

void GBufferPass::createDescriptorSets(uint32_t frameCount)
{
    std::vector<VkDescriptorSetLayout> layouts(frameCount, gbufferDescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = gbufferDescriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();

    gbufferDescriptorSets.resize(frameCount);
    if (vkAllocateDescriptorSets(device, &allocInfo, gbufferDescriptorSets.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate UBO descriptor sets!");
    }

    // 更新描述符集
    for (size_t i = 0; i < frameCount; ++i)
    {
        // 更新 UBO 描述符集
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = vertexResourceManager->getUniformBuffers()[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet uboWrite{};
        uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uboWrite.dstSet = gbufferDescriptorSets[i];
        uboWrite.dstBinding = 0;
        uboWrite.dstArrayElement = 0;
        uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboWrite.descriptorCount = 1;
        uboWrite.pBufferInfo = &bufferInfo;

        std::array<VkWriteDescriptorSet, 1> descriptorWrites = {uboWrite};
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0,
                               nullptr);
    }
}