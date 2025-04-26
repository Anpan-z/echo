#include "shadow_mapping.hpp"
#include "vulkan_utils.hpp"

#include "vertex.hpp"
#include <stdexcept>
#include <array>
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE  
#define GLM_FORCE_DEPTH_ZERO_TO_ONE  
#endif  
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <shaderc/shaderc.hpp>//这个头文件位于VulkanSDK的Include文件夹内
#include <vector>
#include <span>

void ShadowMapping::init(VkDevice device, VkPhysicalDevice physicalDevice, ResourceManager& resourceManager, std::vector<VkCommandBuffer>&& shadowCommandBuffers) {
    this->device = device;
    this->physicalDevice = physicalDevice;
    this->resourceManager = &resourceManager;
    this->shadowCommandBuffers = std::move(shadowCommandBuffers);


    createShadowResources();
    createShadowRenderPass();
    createShadowFramebuffer();
    createShadowDescriptorSetLayout();
    createShadowUniformBuffer();
    createShadowDescriptorPool();
    createShadowDescriptorSet();
    createShadowPipeline();
    createShadowSampler();
}

void ShadowMapping::cleanup() {
    vkDestroySampler(device, shadowSampler, nullptr);
    vkDestroyPipeline(device, shadowPipeline, nullptr);
    vkDestroyPipelineLayout(device, shadowPipelineLayout, nullptr);
    vkDestroyRenderPass(device, shadowRenderPass, nullptr);
    vkDestroyFramebuffer(device, shadowFramebuffer, nullptr);
    vkDestroyImageView(device, shadowDepthImageView, nullptr);
    vkDestroyImage(device, shadowDepthImage, nullptr);
    vkFreeMemory(device, shadowDepthImageMemory, nullptr);

    for (size_t i = 0; i < shadowUniformBuffers.size(); i++) {
        vkDestroyBuffer(device, shadowUniformBuffers[i], nullptr);
        vkFreeMemory(device, shadowUniformBufferMemories[i], nullptr);
    }

    vkDestroyDescriptorSetLayout(device, shadowDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(device, shadowDescriptorPool, nullptr);
}

// 其他函数实现省略，参考主文件中的逻辑

void ShadowMapping::createShadowResources() {
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();
    VkFormat SHADOW_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
    vulkanUtils.createImage(device, physicalDevice, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, VK_SAMPLE_COUNT_1_BIT, SHADOW_DEPTH_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, shadowDepthImage, shadowDepthImageMemory);
    shadowDepthImageView = vulkanUtils.createImageView(device, shadowDepthImage, SHADOW_DEPTH_FORMAT, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void ShadowMapping::createShadowRenderPass() {
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 0;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &depthAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    vkCreateRenderPass(device, &renderPassInfo, nullptr, &shadowRenderPass);
}

void ShadowMapping::createShadowFramebuffer() {
    VkImageView attachments[] = { shadowDepthImageView };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = shadowRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = SHADOW_MAP_WIDTH;
    framebufferInfo.height = SHADOW_MAP_HEIGHT;
    framebufferInfo.layers = 1;

    vkCreateFramebuffer(device, &framebufferInfo, nullptr, &shadowFramebuffer);
}

void ShadowMapping::createShadowDescriptorPool() {
    std::array<VkDescriptorPoolSize, 1> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &shadowDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shadow descriptor pool!");
    }
}

void ShadowMapping::createShadowDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &shadowDescriptorSetLayout);
}

void ShadowMapping::createShadowUniformBuffer() {
    VkDeviceSize bufferSize = sizeof(ShadowUBO);
    shadowUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    shadowUniformBufferMemories.resize(MAX_FRAMES_IN_FLIGHT);
    shadowUniformMappedMemory.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VulkanUtils::getInstance().createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            shadowUniformBuffers[i], shadowUniformBufferMemories[i]);
        vkMapMemory(device, shadowUniformBufferMemories[i], 0, bufferSize, 0, &shadowUniformMappedMemory[i]);
    }
}

void ShadowMapping::createShadowDescriptorSet() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, shadowDescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = shadowDescriptorPool;
    allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = layouts.data();

    shadowDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(device, &allocInfo, shadowDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = shadowUniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(ShadowUBO);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = shadowDescriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }
}

void ShadowMapping::createShadowPipeline() {
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();
    std::string shadow_vertex_shader_code_path = "../shader/shadow.vert";
    std::string shadow_fragment_shader_code_path = "../shader/shadow.frag";

    std::string vs = vulkanUtils.readFileToString(shadow_vertex_shader_code_path);
    std::string fs = vulkanUtils.readFileToString(shadow_fragment_shader_code_path);

    shaderc::Compiler compiler;
    //编译顶点着色器，参数分别是着色器代码字符串，着色器类型，文件名
    auto vertResult = compiler.CompileGlslToSpv(vs, shaderc_glsl_vertex_shader, shadow_vertex_shader_code_path.c_str());
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

    auto fragResult = compiler.CompileGlslToSpv(fs, shaderc_glsl_fragment_shader, shadow_fragment_shader_code_path.c_str());
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

    VkShaderModule vertShaderModule = vulkanUtils.createShaderModule(device, vsmoduleCreateInfo);
    VkShaderModule fragShaderModule = vulkanUtils.createShaderModule(device, fsmoduleCreateInfo);

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

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)SHADOW_MAP_WIDTH;
    viewport.height = (float)SHADOW_MAP_HEIGHT;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT };

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    //rasterizer.depthBiasConstantFactor = 1.25f;
    //rasterizer.depthBiasClamp = 0.0f;
    //rasterizer.depthBiasSlopeFactor = 1.75f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 0; // 因为没有 color attachment
    colorBlending.pAttachments = nullptr;

    //std::vector<VkDynamicState> dynamicStates = {
    //    VK_DYNAMIC_STATE_VIEWPORT,
    //    VK_DYNAMIC_STATE_SCISSOR,
    //    VK_DYNAMIC_STATE_DEPTH_BIAS
    //};
    //VkPipelineDynamicStateCreateInfo dynamicState{};
    //dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    //dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    //dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &shadowDescriptorSetLayout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &shadowPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shadow pipeline layout!");
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
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = shadowPipelineLayout;
    pipelineInfo.renderPass = shadowRenderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &shadowPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shadow graphics pipeline!");
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

void ShadowMapping::createShadowSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_LESS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    vkCreateSampler(device, &samplerInfo, nullptr, &shadowSampler);
}

void ShadowMapping::updateShadowUniformBuffer(uint32_t currentFrame) {
    ShadowUBO ubo{};
    auto lightPos = glm::vec3(3.0f, 3.0f, 3.0f);
    glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightProjection = glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, 1.0f, 10.0f);
    lightProjection[1][1] *= -1; // 反转Y轴
    ubo.lightSpaceMatrix = lightProjection * lightView * glm::mat4(1.0f);

    memcpy(shadowUniformMappedMemory[currentFrame], &ubo, sizeof(ubo));
}

VkCommandBuffer ShadowMapping::recordShadowCommandBuffer(uint32_t currentFrame) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkCommandBuffer commandBuffer = shadowCommandBuffers[currentFrame];
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
       throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = shadowRenderPass;
    renderPassInfo.framebuffer = shadowFramebuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = { SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT };

    VkClearValue clearValue{};
    clearValue.depthStencil = { 1.0f, 0 };
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline);

    VkBuffer vertexBuffers[] = { resourceManager->getVertexBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, resourceManager->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shadowPipelineLayout,
        0, 1, &shadowDescriptorSets[currentFrame],
        0, nullptr
    );

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(resourceManager->getIndices().size()), 1, 0, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
       throw std::runtime_error("failed to record command buffer!");
    }
    return commandBuffer;
}
