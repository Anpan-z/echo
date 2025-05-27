// path_tracing_pipeline.cpp
#include "path_tracing_pipeline.hpp"
#include "vulkan_utils.hpp"
#include <fstream>
#include <vector>
#include <stdexcept>
#include <span>
#include <shaderc/shaderc.hpp>

void PathTracingPipeline::init(VkDevice device, VkPhysicalDevice physicalDevice, PathTracingResourceManager& pathTracingResourceManager, std::vector<VkCommandBuffer>&& commandBuffers) {
    this->device = device;
    this->physicalDevice = physicalDevice;
    this->pathTracingResourceManager = &pathTracingResourceManager;
    this->pathTracingCommandBuffers = std::move(commandBuffers);

    createDescriptorSetLayout();
    createPathTracingPipeline();
    createDescriptorPool();
    createDescriptorSets();

    pathTracingPipelineObserver = std::make_unique<PathTracingPipelineObserver>(this);
    pathTracingResourceManager.addPathTracingResourceReloadObserver(pathTracingPipelineObserver.get());
}

void PathTracingPipeline::cleanup() {
    if (pathTracingPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pathTracingPipeline, nullptr);
    }
    if (pathTracingPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pathTracingPipelineLayout, nullptr);
    }
    if (imageDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, imageDescriptorSetLayout, nullptr);
    }
    if (frameDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, frameDescriptorSetLayout, nullptr);
    }
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    }
}

void PathTracingPipeline::updateOutputImageDescriptorSet() {
    for (size_t i = 0; i < pathTracingResourceManager->getPathTracingOutputImages().size(); i++) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.imageView = pathTracingResourceManager->getPathTracingOutputImageviews()[i];
        imageInfo.sampler = VK_NULL_HANDLE;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = imageDescriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }
}

void PathTracingPipeline::updateStorageBufferDescriptorSet() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo trianglesBufferInfo{};
        trianglesBufferInfo.buffer = pathTracingResourceManager->getTriangleStorageBuffer();
        trianglesBufferInfo.offset = 0;
        trianglesBufferInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo bvhBufferInfo{};
        bvhBufferInfo.buffer = pathTracingResourceManager->getBVHStorageBuffer();
        bvhBufferInfo.offset = 0;
        bvhBufferInfo.range = VK_WHOLE_SIZE;    

        VkDescriptorBufferInfo materialBufferInfo{};
        materialBufferInfo.buffer = pathTracingResourceManager->getMaterialUniformBuffers()[i];
        materialBufferInfo.offset = 0;
        materialBufferInfo.range = sizeof(MaterialUniformBufferObject) * pathTracingResourceManager->getShapeNames().size();

        VkWriteDescriptorSet trianglesWrite{};
        trianglesWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        trianglesWrite.dstSet = frameDescriptorSets[i];
        trianglesWrite.dstBinding = 0; // Triangles 绑定点
        trianglesWrite.dstArrayElement = 0;
        trianglesWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        trianglesWrite.descriptorCount = 1;
        trianglesWrite.pBufferInfo = &trianglesBufferInfo;

        VkWriteDescriptorSet bvhBufferWrite{};
        bvhBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        bvhBufferWrite.dstSet = frameDescriptorSets[i];
        bvhBufferWrite.dstBinding = 1;
        bvhBufferWrite.dstArrayElement = 0;
        bvhBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bvhBufferWrite.descriptorCount = 1;
        bvhBufferWrite.pBufferInfo = &bvhBufferInfo;

        VkWriteDescriptorSet materialsWrite{};
        materialsWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        materialsWrite.dstSet = frameDescriptorSets[i];
        materialsWrite.dstBinding = 2; // Materials 绑定点
        materialsWrite.dstArrayElement = 0;
        materialsWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        materialsWrite.descriptorCount = 1;
        materialsWrite.pBufferInfo = &materialBufferInfo;

        std::vector<VkWriteDescriptorSet> descriptorWrites = {trianglesWrite, bvhBufferWrite, materialsWrite};
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

VkCommandBuffer PathTracingPipeline::recordCommandBuffer(uint32_t frameIndex, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkCommandBuffer commandBuffer = pathTracingCommandBuffers[frameIndex];
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }
    
    // 添加布局转换：从 SHADER_READ_ONLY_OPTIMAL 到 GENERAL
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = pathTracingResourceManager->getPathTracingOutputImages()[imageIndex];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT; // 着色器读取
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT; // 计算着色器写入

    // barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT; // 着色器读取
    // barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_SHADER_READ_BIT; // 计算着色器写入

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // 源阶段
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // 目标阶段
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pathTracingPipeline);

    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pathTracingPipelineLayout,
        0, // Set 0
        1,
        &imageDescriptorSets[imageIndex],
        0,
        nullptr
    );
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pathTracingPipelineLayout,
        1, // Set 1
        1,
        &frameDescriptorSets[frameIndex],
        0,
        nullptr
    );

    // 假设你根据输出图像尺寸来决定工作组数：
    VkExtent2D imageExtent = pathTracingResourceManager->getOutputExtent();
    uint32_t localSizeX = 16; // 计算着色器中定义的工作组大小
    uint32_t localSizeY = 16; // 计算着色器中定义的工作组大小
    uint32_t groupCountX = (imageExtent.width + localSizeX - 1) / localSizeX;
    uint32_t groupCountY = (imageExtent.height + localSizeY - 1) / localSizeY;

    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

    // 添加布局转换：从 GENERAL 到 SHADER_READ_ONLY_OPTIMAL
    // VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = pathTracingResourceManager->getPathTracingOutputImages()[imageIndex];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; // 计算着色器写入
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;  // 着色器读取

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // 源阶段
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // 目标阶段
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }

    return commandBuffer;
}


void PathTracingPipeline::createPathTracingPipeline() {
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();
    std::string compute_shader_code_path = "../shader/pathtracer_lambertian_bvh.comp";
    // std::string compute_shader_code_path = "../shader/pathTracer_lambertian.comp";

    std::string cs = vulkanUtils.readFileToString(compute_shader_code_path);
    shaderc::Compiler compiler;
    //编译顶点着色器，参数分别是着色器代码字符串，着色器类型，文件名
    auto computeResult = compiler.CompileGlslToSpv(cs, shaderc_glsl_compute_shader, compute_shader_code_path.c_str());
    auto errorInfo_vert = computeResult.GetErrorMessage();
    if (!errorInfo_vert.empty()) {
        throw std::runtime_error("Vertex shader compilation error: " + errorInfo_vert);
    }
    //可以加判断，如果有错误信息(errorInfo!=""),就抛出异常

    std::span<const uint32_t> compute_spv = { computeResult.begin(), size_t(computeResult.end() - computeResult.begin()) * 4 };
    VkShaderModuleCreateInfo csmoduleCreateInfo;// 准备顶点着色器模块创建信息
    csmoduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    csmoduleCreateInfo.pNext = nullptr;// 自定义数据的指针
    csmoduleCreateInfo.flags = 0;
    csmoduleCreateInfo.codeSize = compute_spv.size();// 顶点着色器SPV数据总字节数
    csmoduleCreateInfo.pCode = compute_spv.data(); // 顶点着色器SPV数据


    auto computeShaderModule = vulkanUtils.createShaderModule(device, csmoduleCreateInfo);

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = computeShaderModule;
    shaderStageInfo.pName = "main";

    std::vector<VkDescriptorSetLayout> descriptorSetLayout = { imageDescriptorSetLayout, frameDescriptorSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayout.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayout.data();

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pathTracingPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline layout");
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = pathTracingPipelineLayout;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pathTracingPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline");
    }

    vkDestroyShaderModule(device, computeShaderModule, nullptr);
}

void PathTracingPipeline::createDescriptorSetLayout() {
    // Triangles 缓冲区绑定
    VkDescriptorSetLayoutBinding storageImageBinding{};
    storageImageBinding.binding = 0; 
    storageImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; // 存储图像
    storageImageBinding.descriptorCount = 1;
    storageImageBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT; // 仅在计算着色器中使用
    storageImageBinding.pImmutableSamplers = nullptr; // 不使用采样器

    VkDescriptorSetLayoutCreateInfo set0LayoutInfo{};
    set0LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set0LayoutInfo.bindingCount = 1;
    set0LayoutInfo.pBindings = &storageImageBinding;

    if (vkCreateDescriptorSetLayout(device, &set0LayoutInfo, nullptr, &imageDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout for set 0!");
    }

    VkDescriptorSetLayoutBinding triangleBinding{};
    triangleBinding.binding = 0;
    triangleBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    triangleBinding.descriptorCount = 1;
    triangleBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    triangleBinding.pImmutableSamplers = nullptr;

    // BVH 缓冲区绑定
    VkDescriptorSetLayoutBinding bvhBufferBinding{};
    bvhBufferBinding.binding = 1;
    bvhBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bvhBufferBinding.descriptorCount = 1;
    bvhBufferBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bvhBufferBinding.pImmutableSamplers = nullptr;
    
    // Materials 缓冲区绑定
    VkDescriptorSetLayoutBinding materialBinding{};
    materialBinding.binding = 2;
    materialBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    materialBinding.descriptorCount = 1;
    materialBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    materialBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding cameraDataBinding{};
    cameraDataBinding.binding = 3;
    cameraDataBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraDataBinding.descriptorCount = 1;
    cameraDataBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    cameraDataBinding.pImmutableSamplers = nullptr;
    
    std::array<VkDescriptorSetLayoutBinding, 4> bindings = {triangleBinding, bvhBufferBinding, materialBinding, cameraDataBinding};   
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &frameDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void PathTracingPipeline::createDescriptorPool() {
    int imageCount = pathTracingResourceManager->getPathTracingOutputImages().size();
    int frameCount = MAX_FRAMES_IN_FLIGHT;
    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    // VkDescriptorPoolSize poolSize{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[0].descriptorCount = 2 * static_cast<uint32_t>(frameCount);

    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = 2 * static_cast<uint32_t>(frameCount);

    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; // 类型为存储缓冲区
    poolSizes[2].descriptorCount = 1 * static_cast<uint32_t>(imageCount);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = imageCount + frameCount;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void PathTracingPipeline::createDescriptorSets() {
    int imageCount = pathTracingResourceManager->getPathTracingOutputImages().size();
    int frameCount = MAX_FRAMES_IN_FLIGHT;
    std::vector<VkDescriptorSetLayout> layoutsSet0(imageCount, imageDescriptorSetLayout);
    std::vector<VkDescriptorSetLayout> layoutsSet1(frameCount, frameDescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(imageCount);;
    allocInfo.pSetLayouts = layoutsSet0.data();

    imageDescriptorSets.resize(imageCount);
    if (vkAllocateDescriptorSets(device, &allocInfo, imageDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor set!");
    }
    
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(frameCount);;
    allocInfo.pSetLayouts = layoutsSet1.data();

    frameDescriptorSets.resize(frameCount);
    if (vkAllocateDescriptorSets(device, &allocInfo, frameDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor set!");
    }

    //to do: 这里的循环应该是 descriptorSetCount
    for (size_t i = 0; i < imageCount; i++) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // 图像布局必须是 GENERAL
        imageInfo.imageView = pathTracingResourceManager->getPathTracingOutputImageviews()[i]; // 存储图像的视图
        imageInfo.sampler = nullptr; // 如果不需要采样器，可以设置为 nullptr

        VkWriteDescriptorSet imageWrite{};
        imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        imageWrite.dstSet = imageDescriptorSets[i];
        imageWrite.dstBinding = 0; // 对应布局中的绑定点 0
        imageWrite.dstArrayElement = 0;
        imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        imageWrite.descriptorCount = 1;
        imageWrite.pImageInfo = &imageInfo;

        std::vector<VkWriteDescriptorSet> descriptorWrites = {imageWrite};
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    for (size_t i = 0; i < frameCount; i++) {
        VkDescriptorBufferInfo trianglesBufferInfo{};
        trianglesBufferInfo.buffer = pathTracingResourceManager->getTriangleStorageBuffer();
        trianglesBufferInfo.offset = 0;
        trianglesBufferInfo.range = VK_WHOLE_SIZE; 

        VkDescriptorBufferInfo bvhBufferInfo{};
        bvhBufferInfo.buffer = pathTracingResourceManager->getBVHStorageBuffer();
        bvhBufferInfo.offset = 0;
        bvhBufferInfo.range = VK_WHOLE_SIZE;    

        VkDescriptorBufferInfo materialBufferInfo{};
        materialBufferInfo.buffer = pathTracingResourceManager->getMaterialUniformBuffers()[i];
        materialBufferInfo.offset = 0;
        materialBufferInfo.range = sizeof(MaterialUniformBufferObject) * pathTracingResourceManager->getShapeNames().size();

        VkDescriptorBufferInfo cameraDataBufferInfo{};
        cameraDataBufferInfo.buffer = pathTracingResourceManager->getCameraDataBuffer()[i];
        cameraDataBufferInfo.offset = 0;
        cameraDataBufferInfo.range = sizeof(CameraData);        
    
        VkWriteDescriptorSet trianglesWrite{};
        trianglesWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        trianglesWrite.dstSet = frameDescriptorSets[i];
        trianglesWrite.dstBinding = 0; // Triangles 绑定点
        trianglesWrite.dstArrayElement = 0;
        trianglesWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        trianglesWrite.descriptorCount = 1;
        trianglesWrite.pBufferInfo = &trianglesBufferInfo;
    
        VkWriteDescriptorSet bvhBufferWrite{};
        bvhBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        bvhBufferWrite.dstSet = frameDescriptorSets[i];
        bvhBufferWrite.dstBinding = 1;
        bvhBufferWrite.dstArrayElement = 0;
        bvhBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bvhBufferWrite.descriptorCount = 1;
        bvhBufferWrite.pBufferInfo = &bvhBufferInfo;
        
        VkWriteDescriptorSet materialsWrite{};
        materialsWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        materialsWrite.dstSet = frameDescriptorSets[i];
        materialsWrite.dstBinding = 2; // Materials 绑定点
        materialsWrite.dstArrayElement = 0;
        materialsWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        materialsWrite.descriptorCount = 1;
        materialsWrite.pBufferInfo = &materialBufferInfo;
    


        VkWriteDescriptorSet cameraDataWrite{};
        cameraDataWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        cameraDataWrite.dstSet = frameDescriptorSets[i];
        cameraDataWrite.dstBinding = 3; // CameraData 绑定点
        cameraDataWrite.dstArrayElement = 0;
        cameraDataWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraDataWrite.descriptorCount = 1;
        cameraDataWrite.pBufferInfo = &cameraDataBufferInfo;        
    
        std::vector<VkWriteDescriptorSet> descriptorWrites = {trianglesWrite, bvhBufferWrite, materialsWrite, cameraDataWrite};
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}