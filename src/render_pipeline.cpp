#include "render_pipeline.hpp"
#include "vulkan_utils.hpp"
#include "vertex.hpp"
#include <shaderc/shaderc.hpp>
#include <stdexcept>
#include <array>
#include <span>

void RenderPipeline::init(VkDevice device, VkPhysicalDevice physicalDevice, SwapChainManager& swapChainManager, VertexResourceManager& vertexResourceManager,TextureResourceManager& textureResourceManager, std::vector<VkCommandBuffer>&& shadowCommandBuffers) {
    this->device = device;
    this->physicalDevice = physicalDevice;
    this->swapChainManager = &swapChainManager;
    this->vertexResourceManager = &vertexResourceManager;
    this->textureResourceManager = &textureResourceManager;
    this->commandBuffers = std::move(shadowCommandBuffers);

    pipelineModelReloadObserver = std::make_unique<RenderPipelineModelObserver>(this);
    vertexResourceManager.addModelReloadObserver(pipelineModelReloadObserver.get());
    createRenderPass();
}

void RenderPipeline::cleanup() {
    if (graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        graphicsPipeline = VK_NULL_HANDLE;
    }

    if (skyboxPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, skyboxPipeline, nullptr);
        skyboxPipeline = VK_NULL_HANDLE;
    }

    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
    
    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }
    
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    pipelineModelReloadObserver.reset();
}

void RenderPipeline::setup(ShadowMapping& shadowMapping) {
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createSkyboxPipeline();
    // createFramebuffers(swapChainImageViews);
    // createDepthResources();
    createDescriptorPool(MAX_FRAMES_IN_FLIGHT);
    createDescriptorSets(MAX_FRAMES_IN_FLIGHT, shadowMapping);
}

void RenderPipeline::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainManager->getSwapChainImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    // colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VulkanUtils::getInstance().findDepthFormat(physicalDevice);
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription mainSubpass{};
    mainSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    mainSubpass.colorAttachmentCount = 1;
    mainSubpass.pColorAttachments = &colorAttachmentRef;
    mainSubpass.pDepthStencilAttachment = &depthAttachmentRef;

    // 天空盒子子通道
    VkSubpassDescription skyboxSubpass{};
    skyboxSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    skyboxSubpass.colorAttachmentCount = 1;
    skyboxSubpass.pColorAttachments = &colorAttachmentRef;
    skyboxSubpass.pDepthStencilAttachment = nullptr;
    // skyboxSubpass.pDepthStencilAttachment = &depthAttachmentRef;
    
    std::array<VkSubpassDescription, 2> subpasses = { skyboxSubpass, mainSubpass };

    VkSubpassDependency dependency{};
    dependency.srcSubpass = 0; // 天空盒子子通道
    dependency.dstSubpass = 1; // 主渲染子通道
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    // VkSubpassDependency dependency{};
    // dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    // dependency.dstSubpass = 0;
    // dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    // dependency.srcAccessMask = 0;
    // dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    // dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
    renderPassInfo.pSubpasses = subpasses.data();
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void RenderPipeline::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    //VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    //samplerLayoutBinding.binding = 1;
    //samplerLayoutBinding.descriptorCount = 1;
    //samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    //samplerLayoutBinding.pImmutableSamplers = nullptr;
    //samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding shadowMapBinding{};
    shadowMapBinding.binding = 1;
    shadowMapBinding.descriptorCount = 1;
    shadowMapBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    shadowMapBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    shadowMapBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding materialBinding{};
    materialBinding.binding = 2;
    materialBinding.descriptorCount = 1;
    materialBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    materialBinding.pImmutableSamplers = nullptr;
    materialBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding skyBoxBinding{};
    skyBoxBinding.binding = 3;
    skyBoxBinding.descriptorCount = 1;
    skyBoxBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    skyBoxBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    skyBoxBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding irradianceMapBinding{};
    irradianceMapBinding.binding = 4;
    irradianceMapBinding.descriptorCount = 1;
    irradianceMapBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    irradianceMapBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    irradianceMapBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding prefilteredMapBinding{};
    prefilteredMapBinding.binding = 5;
    prefilteredMapBinding.descriptorCount = 1;
    prefilteredMapBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    prefilteredMapBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    prefilteredMapBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding brdfLUBinding{};
    brdfLUBinding.binding = 6;
    brdfLUBinding.descriptorCount = 1;
    brdfLUBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    brdfLUBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    brdfLUBinding.pImmutableSamplers = nullptr;


    std::array<VkDescriptorSetLayoutBinding, 7> bindings = { uboLayoutBinding, shadowMapBinding, materialBinding, skyBoxBinding,
                                                            irradianceMapBinding, prefilteredMapBinding, brdfLUBinding };   
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void RenderPipeline::createGraphicsPipeline() {
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();

    std::string vertex_shader_code_path = "../shader/shader.vert";
    std::string fragment_shader_code_path = "../shader/cook_torrance.frag";

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

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE; // 启用双面绘制，不剔除背面
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;//顺时针为正面
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;//启用深度测试
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 1;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

VkCommandBuffer RenderPipeline::recordCommandBuffer(uint32_t imageIndex, VkFramebuffer swapChainFramebuffers) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkCommandBuffer commandBuffer = commandBuffers[imageIndex];
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapChainManager->getSwapChainExtent();

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // 绑定天空盒管线
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);
    // 设置视口
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapChainManager->getSwapChainExtent().width;
    viewport.height = (float)swapChainManager->getSwapChainExtent().height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapChainManager->getSwapChainExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[imageIndex], 0, nullptr);

    VkBuffer skyBoxVertexBuffers[] = { vertexResourceManager->getSkyBoxVertexBuffer() };
    VkDeviceSize offsets_skybox[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, skyBoxVertexBuffers, offsets_skybox);

    vkCmdDraw(commandBuffer, 36, 1, 0, 0);

    // 切换到主渲染子通道
    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    VkBuffer vertexBuffers[] = { vertexResourceManager->getVertexBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, vertexResourceManager->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[imageIndex], 0, nullptr);

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(vertexResourceManager->getIndices().size()), 1, 0, 0, 0);
    //vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
    return commandBuffer;
}

void RenderPipeline::createDescriptorPool(size_t MAX_FRAMES_IN_FLIGHT) {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2 + 3 * 2;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2 + 3 * 2;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2 + 3 * 2;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void RenderPipeline::createDescriptorSets(size_t MAX_FRAMES_IN_FLIGHT, ShadowMapping& shadowMapping) {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = vertexResourceManager->getUniformBuffers()[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorBufferInfo materialBufferInfo{};
        materialBufferInfo.buffer = vertexResourceManager->getMaterialUniformBuffers()[i];
        materialBufferInfo.offset = 0;
        materialBufferInfo.range = sizeof(MaterialUniformBufferObject) * vertexResourceManager->getShapeNames().size();
        //VkDescriptorImageInfo imageInfo{};
        //imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        //imageInfo.imageView = textureImageView;
        //imageInfo.sampler = textureSampler;

        VkDescriptorImageInfo shadowImageInfo{};
        shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        shadowImageInfo.imageView = shadowMapping.getShadowDepthImageView();
        shadowImageInfo.sampler = shadowMapping.getShadowSampler();

        VkDescriptorImageInfo skyBoxImageInfo{};
        skyBoxImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        skyBoxImageInfo.imageView = textureResourceManager->getEnvironmentMapImageView();
        skyBoxImageInfo.sampler = textureResourceManager->getEnvironmentMapSampler();

        VkDescriptorImageInfo irradianceMapInfo{};
        irradianceMapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        irradianceMapInfo.imageView = textureResourceManager->getIrradianceMapImageView();
        irradianceMapInfo.sampler = textureResourceManager->getIrradianceMapSampler();
        
        VkDescriptorImageInfo prefilteredMapInfo{};
        prefilteredMapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        prefilteredMapInfo.imageView = textureResourceManager->getPrefilteredMapImageView();
        prefilteredMapInfo.sampler = textureResourceManager->getPrefilteredMapSampler();
        
        VkDescriptorImageInfo brdfLUTInfo{};
        brdfLUTInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        brdfLUTInfo.imageView = textureResourceManager->getBRDFLUTImageView();
        brdfLUTInfo.sampler = textureResourceManager->getBRDFLUTSampler();
        
        std::array<VkWriteDescriptorSet, 7> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &shadowImageInfo;

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = descriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &materialBufferInfo;

        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = descriptorSets[i];
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pImageInfo = &skyBoxImageInfo;
        
        descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[4].dstSet = descriptorSets[i];
        descriptorWrites[4].dstBinding = 4;
        descriptorWrites[4].dstArrayElement = 0;
        descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].pImageInfo = &irradianceMapInfo;

        descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[5].dstSet = descriptorSets[i];
        descriptorWrites[5].dstBinding = 5;
        descriptorWrites[5].dstArrayElement = 0;
        descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[5].descriptorCount = 1;
        descriptorWrites[5].pImageInfo = &prefilteredMapInfo;

        descriptorWrites[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[6].dstSet = descriptorSets[i];
        descriptorWrites[6].dstBinding = 6;
        descriptorWrites[6].dstArrayElement = 0;
        descriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[6].descriptorCount = 1;
        descriptorWrites[6].pImageInfo = &brdfLUTInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}


void RenderPipeline::updateMaterialDescriptorSets() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = vertexResourceManager->getUniformBuffers()[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorBufferInfo materialBufferInfo{};
        materialBufferInfo.buffer = vertexResourceManager->getMaterialUniformBuffers()[i];
        materialBufferInfo.offset = 0;
        materialBufferInfo.range = sizeof(MaterialUniformBufferObject) * vertexResourceManager->getShapeNames().size();

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 2;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &materialBufferInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

void RenderPipeline::createSkyboxPipeline() {
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();

    std::string vertex_shader_code_path = "../shader/skybox.vert";
    std::string fragment_shader_code_path = "../shader/skybox.frag";

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

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    // 顶点输入
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getSkyBoxAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // 输入装配
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // 光栅化
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    // rasterizer.cullMode = VK_CULL_MODE_NONE; 
    rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT; // 剔除正面
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    // rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; //顺时针为正面
    rasterizer.lineWidth = 1.0f; // 设置为 1.0

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // 深度测试
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE; // 启用深度测试
    depthStencil.depthWriteEnable = VK_FALSE; // 不写入深度缓冲
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    // 颜色混合
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;


    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();
    // VkViewport viewport{};
    // viewport.x = 0.0f;
    // viewport.y = 0.0f;
    // viewport.width = static_cast<float>(swapChainManager->getSwapChainExtent().width);
    // viewport.height = static_cast<float>(swapChainManager->getSwapChainExtent().height);
    // viewport.minDepth = 0.0f;
    // viewport.maxDepth = 1.0f;
    
    // VkRect2D scissor{};
    // scissor.offset = {0, 0};
    // scissor.extent = swapChainManager->getSwapChainExtent();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr; // 如果启用了动态视口，则可以为 nullptr
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr; // 如果启用了动态裁剪，则可以为 nullptr
    // VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    // pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    // pipelineLayoutInfo.setLayoutCount = 1;
    // pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    // VkPipelineLayout skyBoxPipelineLayout;
    // if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &skyBoxPipelineLayout) != VK_SUCCESS) {
    //     throw std::runtime_error("failed to create pipeline layout!");
    // }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;//启用深度测试
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &skyboxPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
}