
#include "svg_filter_pass.hpp"
#include "vulkan_utils.hpp"
#include <array>
#include <shaderc/shaderc.hpp>
#include <span>
#include <vector>

void SVGFilterPass::init(VkDevice device, VkPhysicalDevice physicalDevice, SwapChainManager& swapChainManager,
                         GBufferResourceManager& gbufferResourceManager,
                         PathTracingResourceManager& pathTracingResourceManager,
                         SVGFilterResourceManager& svgFilterResourceManager,
                         std::vector<VkCommandBuffer>&& commandBuffers)
{
    this->device = device;
    this->physicalDevice = physicalDevice;
    this->swapChainManager = &swapChainManager;
    this->gbufferResourceManager = &gbufferResourceManager;
    this->pathTracingResourceManager = &pathTracingResourceManager;
    this->svgFilterResourceManager = &svgFilterResourceManager;
    this->imageCount = swapChainManager.getSwapChainImages().size();
    this->commandBuffers = std::move(commandBuffers);

    createDescriptorSetLayout();
    createPipeline();
    createDescriptorPool();
    createDescriptorSets();
}

void SVGFilterPass::cleanup()
{
    if (pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
    if (descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        descriptorSetLayout = VK_NULL_HANDLE;
    }
    if (descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
    descriptorSets.clear();
}

void SVGFilterPass::createDescriptorSetLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 4> bindings = {};

    // binding 0: pathTracedColorBuffer (sampler2D)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    // binding 1: gbufferNormalBuffer (sampler2D)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    // binding 2: gbufferDepthBuffer (sampler2D)
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[2].pImmutableSamplers = nullptr;

    // binding 3: denoisedOutputImage (image2D - storage image)
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[3].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create SVG filter descriptor set layout!");
    }
}

void SVGFilterPass::createPipeline()
{
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();
    std::string compute_shader_code_path = "../shader/svg_filter.comp";

    std::string cs = vulkanUtils.readFileToString(compute_shader_code_path);
    shaderc::Compiler compiler;
    // 编译顶点着色器，参数分别是着色器代码字符串，着色器类型，文件名
    auto computeResult = compiler.CompileGlslToSpv(cs, shaderc_glsl_compute_shader, compute_shader_code_path.c_str());
    auto errorInfo_vert = computeResult.GetErrorMessage();

    if (!errorInfo_vert.empty())
    {
        throw std::runtime_error("Vertex shader compilation error: " + errorInfo_vert);
    }

    std::span<const uint32_t> compute_spv = {computeResult.begin(),
                                             size_t(computeResult.end() - computeResult.begin()) * 4};
    VkShaderModuleCreateInfo csmoduleCreateInfo; // 准备计算着色器模块创建信息
    csmoduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    csmoduleCreateInfo.pNext = nullptr; // 自定义数据的指针
    csmoduleCreateInfo.flags = 0;
    csmoduleCreateInfo.codeSize = compute_spv.size(); // 计算着色器SPV数据总字节数
    csmoduleCreateInfo.pCode = compute_spv.data();    // 计算着色器SPV数据

    auto computeShaderModule = vulkanUtils.createShaderModule(device, csmoduleCreateInfo);

    // 2. 创建管线布局
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    // pipelineLayoutInfo.pushConstantRangeCount = 0; // 如果不用 Push Constants

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        vkDestroyShaderModule(device, computeShaderModule, nullptr);
        throw std::runtime_error("Failed to create SVG filter pipeline layout!");
    }

    // 3. 创建 Compute Pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = computeShaderModule;
    pipelineInfo.stage.pName = "main"; // Shader 入口函数名
    // pipelineInfo.flags = 0;
    // pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    // pipelineInfo.basePipelineIndex = -1;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
    {
        vkDestroyShaderModule(device, computeShaderModule, nullptr);
        throw std::runtime_error("Failed to create SVG filter compute pipeline!");
    }

    // 创建完管线后，Shader Module 可以销毁
    vkDestroyShaderModule(device, computeShaderModule, nullptr);
}

void SVGFilterPass::createDescriptorPool()
{
    uint32_t numFrames = imageCount;
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    // Sampler2D (Combined Image Sampler)
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 3 * numFrames; // 3 samplers per frame
    // Image2D (Storage Image)
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 1 * numFrames; // 1 storage image per frame

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = numFrames;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; // 可选，如果需要单独释放

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create SVG filter descriptor pool!");
    }
}

void SVGFilterPass::createDescriptorSets()
{
    descriptorSets.resize(imageCount, VK_NULL_HANDLE);

    std::vector<VkDescriptorSetLayout> layouts(imageCount, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate SVG filter descriptor sets!");
    }

    for (size_t imageIndex = 0; imageIndex < descriptorSets.size(); ++imageIndex)
    {
        std::array<VkWriteDescriptorSet, 4> descriptorWrites{};

        // 0: pathTracedColorBuffer
        VkDescriptorImageInfo pathTracedColorInfo{};
        pathTracedColorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        pathTracedColorInfo.imageView = pathTracingResourceManager->getPathTracingOutputImageviews()[imageIndex];
        pathTracedColorInfo.sampler = svgFilterResourceManager->getDenoiserInputSampler(); // 或者特定的采样器

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[imageIndex];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &pathTracedColorInfo;

        // 1: gbufferNormalBuffer
        VkDescriptorImageInfo gbufferNormalInfo{};
        gbufferNormalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        gbufferNormalInfo.imageView = gbufferResourceManager->getNormalAttachment(imageIndex).view;
        gbufferNormalInfo.sampler = svgFilterResourceManager->getDenoiserInputSampler();

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[imageIndex];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &gbufferNormalInfo;

        // 2: gbufferDepthBuffer
        VkDescriptorImageInfo gbufferDepthInfo{};
        gbufferDepthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        // gbufferDepthInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        gbufferDepthInfo.imageView = gbufferResourceManager->getDepthAttachment(imageIndex).view;
        gbufferDepthInfo.sampler = svgFilterResourceManager->getDenoiserInputSampler();

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = descriptorSets[imageIndex];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pImageInfo = &gbufferDepthInfo;

        // 3: denoisedOutputImage
        VkDescriptorImageInfo denoisedOutputInfo{};
        denoisedOutputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // Storage images 通常使用 GENERAL 布局
        denoisedOutputInfo.imageView = svgFilterResourceManager->getDenoisedOutputImageView()[imageIndex];
        // denoisedOutputInfo.sampler = VK_NULL_HANDLE; // Storage images 不需要 sampler

        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = descriptorSets[imageIndex];
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pImageInfo = &denoisedOutputInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0,
                               nullptr);
    }
}

VkCommandBuffer SVGFilterPass::recordCommandBuffer(uint32_t frameIndex, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkCommandBuffer commandBuffer = commandBuffers[frameIndex];
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    // 添加布局转换：从 SHADER_READ_ONLY_OPTIMAL 到 GENERAL
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = svgFilterResourceManager->getDenoisedOutputImage()[imageIndex]; // 获取图像
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;  // 着色器读取
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT; // 计算着色器写入

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // 源阶段
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // 目标阶段
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    // 绑定 Compute Pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    // 绑定当前帧的描述符集
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1,
                            &descriptorSets[imageIndex], 0, nullptr);

    // 调度 Compute Shader
    VkExtent2D imageExtent = svgFilterResourceManager->getOutputExtent();
    uint32_t localSizeX = 16; // 计算着色器中定义的工作组大小
    uint32_t localSizeY = 16; // 计算着色器中定义的工作组大小
    uint32_t groupCountX = (imageExtent.width + localSizeX - 1) / localSizeX;
    uint32_t groupCountY = (imageExtent.height + localSizeY - 1) / localSizeY;
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

    // 添加布局转换：从 GENERAL 到 SHADER_READ_ONLY_OPTIMAL
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = svgFilterResourceManager->getDenoisedOutputImage()[imageIndex]; // 获取图像
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; // 计算着色器写入
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;  // 着色器读取

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // 源阶段
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // 目标阶段
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to record command buffer!");
    }

    return commandBuffer;
}