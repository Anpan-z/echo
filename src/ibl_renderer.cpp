#include "ibl_renderer.hpp"
#include "vulkan_utils.hpp"
#include <shaderc/shaderc.hpp>
#include <span>
#include <stdexcept>

void IBLRenderer::init(VkDevice device, VkQueue graphicsQueue, CommandManager& commandManager, TextureResourceManager& textureResourceManager) {
    this->device = device;
    this->graphicsQueue = graphicsQueue;
    this->commandManager = &commandManager;


    createSampler();
    createDescriptorPool();
    createDescriptorSetLayout();
    generateEnvironmentMap(textureResourceManager);
    // generateIrradianceMap(textureResourceManager);
}

void IBLRenderer::cleanup() {
    vkDestroySampler(device, environmentMapSampler, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    // vkDestroyPipelineLayout(device, irradianceMapPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
}

void IBLRenderer::generateEnvironmentMap(TextureResourceManager& textureResourceManager) {
    // VkImage environmentMapImage = textureResourceManager.getEnvironmentMapImage();
    VkImageView sourceHDRImageView = textureResourceManager.getSourceHDRImageView();
    VkImageView environmentMapImageView = textureResourceManager.getEnvironmentMapImageView();
    std::array<VkImageView, 6> environmentMapFaceImageViews = textureResourceManager.getEnvironmentMapFaceImageViews();

    const uint32_t environmentMapSize = 512;

    VkRenderPass renderPass = createRenderPass(VK_FORMAT_R32G32B32A32_SFLOAT);
    std::string vertex_shader_code_path = "../shader/ibl_environment.vert";
    std::string fragment_shader_code_path = "../shader/ibl_environment.frag";
    // VkPipelineLayout environmentMapPipelineLayout;
    auto [environmentPipeline, environmentMapPipelineLayout] = createPipeline(renderPass, environmentMapSize, vertex_shader_code_path, fragment_shader_code_path);
    
    
    for (size_t i = 0; i < environmentMapFaceImageViews.size(); ++i) {
        VkFramebuffer framebuffer = createFramebuffer(environmentMapFaceImageViews[i], renderPass, environmentMapSize, environmentMapSize);
        VkCommandBuffer commandBuffer = commandManager->beginSingleTimeCommands();
        
        VkClearValue clearValue{};
        clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = framebuffer;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = {environmentMapSize, environmentMapSize};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearValue;
    
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, environmentPipeline);

        // VkViewport viewport{};
        // viewport.x = 0.0f;
        // viewport.y = 0.0f;
        // viewport.width  = (float)environmentMapSize;
        // viewport.height = (float)environmentMapSize;
        // viewport.minDepth = 0.0f;
        // viewport.maxDepth = 1.0f;
        // vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        
        // VkRect2D scissor{};
        // scissor.offset = {0, 0};
        // scissor.extent = {environmentMapSize, environmentMapSize};
        // vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
        // Bind environment map as input
        VkDescriptorSet descriptorSet = createDescriptorSet(sourceHDRImageView); // Create descriptor set for environment map
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, environmentMapPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    
        vkCmdDraw(commandBuffer, 6, 1, 6*i, 0);
        vkCmdEndRenderPass(commandBuffer);

        commandManager->endSingleTimeCommands(commandBuffer);

        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    
    vkDestroyPipeline(device, environmentPipeline, nullptr);
    vkDestroyPipelineLayout(device, environmentMapPipelineLayout, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);
}

void IBLRenderer::generateIrradianceMap(TextureResourceManager& textureResourceManager) {
    // VkImage environmentMapImage = textureResourceManager.getEnvironmentMapImage();
    VkImageView environmentMapImageView = textureResourceManager.getEnvironmentMapImageView();

    // VkImage irradianceMapImage = textureResourceManager.getIrradianceMapImage();
    VkImageView irradianceMapImageView = textureResourceManager.getIrradianceMapImageView();

    const uint32_t irradianceMapSize = 32;

    VkRenderPass renderPass = createRenderPass(VK_FORMAT_R32G32B32A32_SFLOAT);
    VkFramebuffer framebuffer = createFramebuffer(irradianceMapImageView, renderPass, irradianceMapSize, irradianceMapSize);
    std::string vertex_shader_code_path = "../shader/irradiance.vert";
    std::string fragment_shader_code_path = "../shader/irradiance.frag";
    auto [irradiancePipeline, irradianceMapPipelineLayout] = createPipeline(renderPass, irradianceMapSize, vertex_shader_code_path, fragment_shader_code_path);

    VkCommandBuffer commandBuffer = commandManager->beginSingleTimeCommands();

    VkClearValue clearValue{};
    clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {irradianceMapSize, irradianceMapSize};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irradiancePipeline);

    // Bind environment map as input
    VkDescriptorSet descriptorSet = createDescriptorSet(environmentMapImageView); // Create descriptor set for environment map
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irradianceMapPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    vkCmdDraw(commandBuffer, 6, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);

    commandManager->endSingleTimeCommands(commandBuffer);

    vkDestroyFramebuffer(device, framebuffer, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);
    vkDestroyPipeline(device, irradiancePipeline, nullptr);
}

VkRenderPass IBLRenderer::createRenderPass(VkFormat format) {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkRenderPass renderPass;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass for irradiance map!");
    }

    return renderPass;
}

VkFramebuffer IBLRenderer::createFramebuffer(VkImageView& imageView, VkRenderPass renderPass, uint32_t width, uint32_t height) {
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass; // 使用与渲染管线匹配的 RenderPass
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &imageView;
    framebufferInfo.width = width;
    framebufferInfo.height = height;
    framebufferInfo.layers = 1;

    VkFramebuffer framebuffer;
    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create framebuffer for irradiance map!");
    }

    return framebuffer;
}

std::tuple<VkPipeline, VkPipelineLayout> IBLRenderer::createPipeline(VkRenderPass renderPass, uint32_t size, std::string vertex_shader_code_path, std::string fragment_shader_code_path) {
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();

    // 加载着色器模块
    // std::string vertex_shader_code_path = "../shader/irradiance.vert";
    // std::string fragment_shader_code_path = "../shader/irradiance.frag";

    std::string vs = vulkanUtils.readFileToString(vertex_shader_code_path);
    shaderc::Compiler compiler;
    //编译顶点着色器，参数分别是着色器代码字符串，着色器类型，文件名
    auto vertResult = compiler.CompileGlslToSpv(vs, shaderc_glsl_vertex_shader, vertex_shader_code_path.c_str());
    auto errorInfo_vert = vertResult.GetErrorMessage();
    if (!errorInfo_vert.empty()) {
        throw std::runtime_error("Vertex shader compilation error: " + errorInfo_vert);
    }
    //可以加判断，如果有错误信息(errorInfo!=""),就抛出异常

    std::span<const uint32_t> vert_spv = { vertResult.begin(), size_t(vertResult.end() - vertResult.begin()) * 4 };
    VkShaderModuleCreateInfo vsmoduleCreateInfo;// 准备顶点着色器模块创建信息
    vsmoduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vsmoduleCreateInfo.pNext = nullptr;// 自定义数据的指针
    vsmoduleCreateInfo.flags = 0;
    vsmoduleCreateInfo.codeSize = vert_spv.size();// 顶点着色器SPV数据总字节数
    vsmoduleCreateInfo.pCode = vert_spv.data(); // 顶点着色器SPV数据
    
    std::string fs = vulkanUtils.readFileToString(fragment_shader_code_path);
    auto fragResult = compiler.CompileGlslToSpv(fs, shaderc_glsl_fragment_shader, fragment_shader_code_path.c_str());
    auto errorInfo_frag = fragResult.GetErrorMessage();
    if (!errorInfo_frag.empty()) {
        throw std::runtime_error("Fragment shader compilation error: " + errorInfo_frag);
    }
    //可以加判断，如果有错误信息(errorInfo!=""),就抛出异常
    std::span<const uint32_t> frag_spv = { fragResult.begin(), size_t(fragResult.end() - fragResult.begin()) * 4 };
    VkShaderModuleCreateInfo fsmoduleCreateInfo;// 准备顶点着色器模块创建信息
    fsmoduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fsmoduleCreateInfo.pNext = nullptr;// 自定义数据的指针
    fsmoduleCreateInfo.flags = 0;
    fsmoduleCreateInfo.codeSize = frag_spv.size();// 顶点着色器SPV数据总字节数
    fsmoduleCreateInfo.pCode = frag_spv.data(); // 顶点着色器SPV数据
    //auto vertShaderCode = readFile("shaders/vert.spv");
    //auto fragShaderCode = readFile("shaders/frag.spv");

    auto vertShaderModule = vulkanUtils.createShaderModule(device, vsmoduleCreateInfo);
    auto fragShaderModule = vulkanUtils.createShaderModule(device, fsmoduleCreateInfo);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // 顶点输入（无顶点数据）
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    // 输入装配
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // 视口和剪裁
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(size);
    viewport.height = static_cast<float>(size);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {size, size};

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
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    // rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // 多重采样
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // 颜色混合
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // 动态状态
    // std::vector<VkDynamicState> dynamicStates = {
    //     VK_DYNAMIC_STATE_VIEWPORT,
    //     VK_DYNAMIC_STATE_SCISSOR
    // };
    
    // VkPipelineDynamicStateCreateInfo dynamicState{};
    // dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    // dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    // dynamicState.pDynamicStates = dynamicStates.data();
    
    VkPipelineLayout PipelineLayout;

    // 管线布局
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &PipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout for irradiance map!");
    }

    // 创建图形管线
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    // pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = PipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline for irradiance map!");
    }

    // 清理着色器模块
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);

    return {pipeline, PipelineLayout};
}

void IBLRenderer::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

VkDescriptorSet IBLRenderer::createDescriptorSet(VkImageView environmentMapImageView) {
    // 描述符集布局
    // VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    // samplerLayoutBinding.binding = 0;
    // samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    // samplerLayoutBinding.descriptorCount = 1;
    // samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    // samplerLayoutBinding.pImmutableSamplers = nullptr;

    // VkDescriptorSetLayoutCreateInfo layoutInfo{};
    // layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    // layoutInfo.bindingCount = 1;
    // layoutInfo.pBindings = &samplerLayoutBinding;

    // VkDescriptorSetLayout descriptorSetLayout;
    // if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
    //     throw std::runtime_error("Failed to create descriptor set layout!");
    // }

    // 分配描述符集
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool; // 创建描述符池
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    VkDescriptorSet descriptorSet;
    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set!");
    }

    // 更新描述符集
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = environmentMapImageView;
    imageInfo.sampler = environmentMapSampler; // 创建采样器
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

    return descriptorSet;
}

void IBLRenderer::createDescriptorPool() {
    // 定义描述符池大小
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 6; // 支持 3 个采样器

    // 创建描述符池
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 6; // 最大支持 3 个描述符集

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
}

void IBLRenderer::createSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR; // 放大过滤
    samplerInfo.minFilter = VK_FILTER_LINEAR; // 缩小过滤
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; // Mipmap 线性过滤
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // U 方向边界模式
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // V 方向边界模式
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // W 方向边界模式
    samplerInfo.mipLodBias = 0.0f; // Mipmap 偏移
    samplerInfo.anisotropyEnable = VK_TRUE; // 启用各向异性过滤
    samplerInfo.maxAnisotropy = 16; // 最大各向异性等级
    samplerInfo.compareEnable = VK_FALSE; // 不启用比较操作
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.minLod = 0.0f; // 最小 LOD
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE; // 最大 LOD
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // 边界颜色
    samplerInfo.unnormalizedCoordinates = VK_FALSE; // 使用标准化纹理坐标

    
    if (vkCreateSampler(device, &samplerInfo, nullptr, &environmentMapSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create sampler!");
    }
}