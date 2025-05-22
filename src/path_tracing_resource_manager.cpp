#include "path_tracing_resource_manager.hpp"
#include "vulkan_utils.hpp"
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE  
#define GLM_FORCE_DEPTH_ZERO_TO_ONE  
#endif 
#include <glm/gtc/matrix_transform.hpp>

void PathTracingResourceManager::init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue graphicsQueue, SwapChainManager& swapChainManager,CommandManager& commandManager, VertexResourceManager& vertexResourceManager) {
    this->device = device;
    this->physicalDevice = physicalDevice;
    this->graphicsQueue = graphicsQueue;
    this->swapChainManager = &swapChainManager;
    this->outPutExtent = swapChainManager.getSwapChainExtent();
    this->commandManager = &commandManager;
    this->vertexResourceManager = &vertexResourceManager;
    this->materialUniformBuffers = &vertexResourceManager.getMaterialUniformBuffers();
    
    buildTrianglesFromMesh(vertexResourceManager.getVertices(), vertexResourceManager.getIndices());
    createStorageBuffer();
    createPathTracingOutputImages();
    createCameraDataBuffer();
}

void PathTracingResourceManager::cleanup() {
    vkDestroyBuffer(device, storageBuffer, nullptr);
    vkFreeMemory(device, storageBufferMemory, nullptr);

    for (size_t i = 0; i < storageImages.size(); i++) {
        vkDestroyImageView(device, storageImageViews[i], nullptr);
        vkDestroyImage(device, storageImages[i], nullptr);
        vkFreeMemory(device, storageImageMemories[i], nullptr);
    }
    for (size_t i = 0; i < cameraDataBuffer.size(); i++) {
        vkDestroyBuffer(device, cameraDataBuffer[i], nullptr);
        vkFreeMemory(device, cameraDataBufferMemory[i], nullptr);
    }
}

void PathTracingResourceManager::recreatePathTracingOutputImages() {
    vkDeviceWaitIdle(device);
    for (size_t i = 0; i < storageImages.size(); i++) {
        vkDestroyImageView(device, storageImageViews[i], nullptr);
        vkDestroyImage(device, storageImages[i], nullptr);
        vkFreeMemory(device, storageImageMemories[i], nullptr);
    }
    createPathTracingOutputImages();
    outPutExtent = swapChainManager->getSwapChainExtent();
    totalSampleCount = 0;
}

void PathTracingResourceManager::buildTrianglesFromMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    
    size_t triangleCount = indices.size() / 3;

    for (size_t i = 0; i < triangleCount; ++i) {
        const Vertex& v0 = vertices[indices[i * 3 + 0]];
        const Vertex& v1 = vertices[indices[i * 3 + 1]];
        const Vertex& v2 = vertices[indices[i * 3 + 2]];

        glm::vec3 edge1 = v1.pos - v0.pos;
        glm::vec3 edge2 = v2.pos - v0.pos;
        glm::vec3 faceNormal = glm::normalize(v0.normal + v1.normal + v2.normal); // 使用顶点法线的平均值
        // glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));

        Triangle tri;
        tri.v0 = v0.pos;
        tri.v1 = v1.pos;
        tri.v2 = v2.pos;
        tri.normal = faceNormal; // 或者用 v0.normal，依据你想用哪种法线
        tri.materialID = v0.materialID; // 默认 v0 的材质 ID，一般 OBJ 同面材质一致

        triangles.push_back(tri);
    }
}

void PathTracingResourceManager::createStorageBuffer(){
    VkDeviceSize bufferSize = sizeof(triangles[0]) * triangles.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();
    vulkanUtils.createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, triangles.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    vulkanUtils.createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, storageBuffer, storageBufferMemory);

    vulkanUtils.copyBuffer(device, commandManager->getCommandPool(), graphicsQueue, stagingBuffer, storageBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void PathTracingResourceManager::createPathTracingOutputImages(){
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();
    storageImages.resize(swapChainManager->getSwapChainImageViews().size());
    storageImageMemories.resize(storageImages.size());
    storageImageViews.resize(storageImages.size());

    for (size_t i = 0; i < storageImages.size(); i++) {
        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
        // VkFormat format = swapChainManager->getSwapChainImageFormat();
        vulkanUtils.createImage(device, physicalDevice, swapChainManager->getSwapChainExtent().width, swapChainManager->getSwapChainExtent().height, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, storageImages[i], storageImageMemories[i]);
        vulkanUtils.transitionImageLayout(device, commandManager->getCommandPool(), graphicsQueue, storageImages[i], format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        storageImageViews[i] = vulkanUtils.createImageView(device, storageImages[i], format, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void PathTracingResourceManager::createCameraDataBuffer() {
    VkDeviceSize bufferSize = sizeof(CameraData);

    cameraDataBuffer.resize(swapChainManager->getSwapChainImageViews().size());
    cameraDataBufferMemory.resize(cameraDataBuffer.size());
    cameraDataBuffersMapped.resize(cameraDataBuffer.size());

    for (size_t i = 0; i < cameraDataBuffer.size(); i++) {
        VulkanUtils& vulkanUtils = VulkanUtils::getInstance();
        vulkanUtils.createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, cameraDataBuffer[i], cameraDataBufferMemory[i]);
        vkMapMemory(device, cameraDataBufferMemory[i], 0, bufferSize, 0, &cameraDataBuffersMapped[i]);
    }
}

void PathTracingResourceManager::updateCameraDataBuffer(uint32_t currentFrame, VkExtent2D swapChainExtent, Camera& camera) {
    CameraData cameraData;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);
    proj[1][1] *= -1; // flip Y axis for Vulkan
    cameraData.invViewProj = glm::inverse(proj * camera.getViewMatrix());
    cameraData.cameraPos = camera.getPosition();
    cameraData.frame = totalSampleCount++;

    memcpy(cameraDataBuffersMapped[currentFrame], &cameraData, sizeof(CameraData));
}