#include "path_tracing_resource_manager.hpp"
#include "vulkan_utils.hpp"
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE  
#define GLM_FORCE_DEPTH_ZERO_TO_ONE  
#endif 
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

void PathTracingResourceManager::init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue graphicsQueue, SwapChainManager& swapChainManager,CommandManager& commandManager, VertexResourceManager& vertexResourceManager) {
    this->device = device;
    this->physicalDevice = physicalDevice;
    this->graphicsQueue = graphicsQueue;
    this->swapChainManager = &swapChainManager;
    this->outPutExtent = swapChainManager.getSwapChainExtent();
    this->commandManager = &commandManager;
    this->vertexResourceManager = &vertexResourceManager;
    this->materialUniformBuffers = &vertexResourceManager.getMaterialUniformBuffers();
    //用于rebuild相关资源时双帧或多帧同步
    this->maxFramesInFlight = MAX_FRAMES_IN_FLIGHT;
    this->framesToForceZero = maxFramesInFlight;
    
    buildTrianglesFromMesh(vertexResourceManager.getVertices(), vertexResourceManager.getIndices());
    buildBVH();
    createTriangleStorageBuffer();
    createPathTracingOutputImages();
    createCameraDataBuffer();

    pathTracingResourceManagerModelObserver = std::make_unique<PathTracingResourceManagerModelObserver>(this); // 创建模型重新加载观察者
    vertexResourceManager.addModelReloadObserver(pathTracingResourceManagerModelObserver.get()); // 添加观察者
}

void PathTracingResourceManager::cleanup() {
    vkDestroyBuffer(device, triangleStorageBuffer, nullptr);
    vkFreeMemory(device, triangleStorageBufferMemory, nullptr);

    vkDestroyBuffer(device, emissiveTrianglesBuffer, nullptr);
    vkFreeMemory(device, emissiveTrianglesBufferMemory, nullptr);

    vkDestroyBuffer(device, BVHStorageBuffer, nullptr);
    vkFreeMemory(device, BVHStorageBufferMemory, nullptr);
    
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
    for(auto observer : pathTracingResourceReloadObservers){
        observer->onPathTracingOutputImagesRecreated();
    }
}

void PathTracingResourceManager::recreteTriangleData() {
    triangles.clear();
    bvhNodes.clear();
    vkDestroyBuffer(device, triangleStorageBuffer, nullptr);
    vkFreeMemory(device, triangleStorageBufferMemory, nullptr);
    vkDestroyBuffer(device, BVHStorageBuffer, nullptr);
    vkFreeMemory(device, BVHStorageBufferMemory, nullptr);
    buildTrianglesFromMesh(vertexResourceManager->getVertices(), vertexResourceManager->getIndices());
    buildBVH();
    createTriangleStorageBuffer();
    vkDeviceWaitIdle(device);
    totalSampleCount = 0;
    framesToForceZero = maxFramesInFlight;
    for(auto observer : pathTracingResourceReloadObservers){
        observer->onModelReloaded();
    }
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
        tri.n0 = v0.normal;
        tri.n1 = v1.normal;
        tri.n2 = v2.normal;
        tri.normal = faceNormal; // 或者用 v0.normal，依据你想用哪种法线
        tri.materialID = v0.materialID; // 默认 v0 的材质 ID，一般 OBJ 同面材质一致

        triangles.push_back(tri);
        if (vertexResourceManager->getShapeNames()[tri.materialID] == "light") {
            EmissiveTriangle emissiveTri;
            emissiveTri.triangleIndex = i;
            emissiveTriangles.push_back(emissiveTri);
        }
    }
}

void PathTracingResourceManager::buildBVH() {
    // 包装三角形并计算其 AABB
    std::vector<int> triangleIndices(triangles.size());
    for (size_t i = 0; i < triangles.size(); ++i) {
        triangleIndices[i] = static_cast<int>(i);
    }

    // 构建 BVH
    buildBVHNode(triangles, triangleIndices, 0, triangleIndices.size(), bvhNodes);

    // 将 BVH 数据上传到 GPU
    VkDeviceSize bufferSize = sizeof(BVHNode) * bvhNodes.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();
    vulkanUtils.createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    // 将 BVH 数据复制到 staging buffer
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, bvhNodes.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    // 创建 GPU 专用的 BVH 缓冲区
    vulkanUtils.createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, BVHStorageBuffer, BVHStorageBufferMemory);

    // 将数据从 staging buffer 复制到 GPU 缓冲区
    vulkanUtils.copyBuffer(device, commandManager->getCommandPool(), graphicsQueue, stagingBuffer, BVHStorageBuffer, bufferSize);

    // 清理 staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

int PathTracingResourceManager::buildBVHNode(const std::vector<Triangle>& triangles, std::vector<int>& triangleIndices, int start, int end, std::vector<BVHNode>& bvhNodes) {
    // 创建一个新的 BVH 节点
    BVHNode node;
    node.leftChild = -1;
    node.rightChild = -1;
    node.triangleIndex = -1;

    // 计算当前节点的包围盒
    node.minBounds = glm::vec3(std::numeric_limits<float>::max());
    node.maxBounds = glm::vec3(std::numeric_limits<float>::lowest());
    for (int i = start; i < end; ++i) {
        const Triangle& tri = triangles[triangleIndices[i]];
        node.minBounds = glm::min(node.minBounds, glm::min(glm::min(tri.v0, tri.v1), tri.v2));
        node.maxBounds = glm::max(node.maxBounds, glm::max(glm::max(tri.v0, tri.v1), tri.v2));
    }

    int nodeIndex = static_cast<int>(bvhNodes.size());
    bvhNodes.push_back(node);

    // 如果只有一个三角形，创建叶节点
    if (end - start == 1) {
        bvhNodes[nodeIndex].triangleIndex = triangleIndices[start];
        return nodeIndex;
    }

    // 分割三角形
    int mid = partitionTrianglesSAH(triangles, triangleIndices, start, end);

    // 递归构建子节点
    bvhNodes[nodeIndex].leftChild = buildBVHNode(triangles, triangleIndices, start, mid, bvhNodes);
    bvhNodes[nodeIndex].rightChild = buildBVHNode(triangles, triangleIndices, mid, end, bvhNodes);

    return nodeIndex;
}

int partitionTriangles(const std::vector<Triangle>& triangles, std::vector<int>& triangleIndices, int start, int end) {
    // 使用中位数分割
    int axis = 0; // 固定使用 X 轴分割（可以改进为根据 SAH 选择轴）
    int mid = (start + end) / 2;

    std::nth_element(triangleIndices.begin() + start, triangleIndices.begin() + mid, triangleIndices.begin() + end,
        [&triangles, axis](int a, int b) {
            glm::vec3 centerA = (triangles[a].v0 + triangles[a].v1 + triangles[a].v2) / 3.0f;
            glm::vec3 centerB = (triangles[b].v0 + triangles[b].v1 + triangles[b].v2) / 3.0f;
            return centerA[axis] < centerB[axis];
        });

    return mid;
}

int PathTracingResourceManager::partitionTrianglesSAH(const std::vector<Triangle>& triangles, std::vector<int>& triangleIndices, int start, int end) {
    const int numTriangles = end - start;
    if (numTriangles <= 1) {
        return start; // 无需分割
    }

    // 初始化
    int bestSplit = start;
    float bestCost = std::numeric_limits<float>::max();

    // 遍历每个轴（X、Y、Z）
    for (int axis = 0; axis < 3; ++axis) {
        // 按当前轴对三角形中心排序
        std::sort(triangleIndices.begin() + start, triangleIndices.begin() + end,
            [&triangles, axis](int a, int b) {
                glm::vec3 centerA = (triangles[a].v0 + triangles[a].v1 + triangles[a].v2) / 3.0f;
                glm::vec3 centerB = (triangles[b].v0 + triangles[b].v1 + triangles[b].v2) / 3.0f;
                return centerA[axis] < centerB[axis];
            });

        // 预计算左右子节点的包围盒
        BVHNode leftNode, rightNode;
        leftNode.minBounds = glm::vec3(std::numeric_limits<float>::max());
        leftNode.maxBounds = glm::vec3(std::numeric_limits<float>::lowest());
        rightNode.minBounds = glm::vec3(std::numeric_limits<float>::max());
        rightNode.maxBounds = glm::vec3(std::numeric_limits<float>::lowest());

        std::vector<BVHNode> leftBounds(numTriangles);
        std::vector<BVHNode> rightBounds(numTriangles);

        for (int i = start; i < end; ++i) {
            const Triangle& tri = triangles[triangleIndices[i]];
            leftNode.minBounds = glm::min(leftNode.minBounds, glm::min(glm::min(tri.v0, tri.v1), tri.v2));
            leftNode.maxBounds = glm::max(leftNode.maxBounds, glm::max(glm::max(tri.v0, tri.v1), tri.v2));
            leftBounds[i - start] = leftNode;
        }

        for (int i = end - 1; i >= start; --i) {
            const Triangle& tri = triangles[triangleIndices[i]];
            rightNode.minBounds = glm::min(rightNode.minBounds, glm::min(glm::min(tri.v0, tri.v1), tri.v2));
            rightNode.maxBounds = glm::max(rightNode.maxBounds, glm::max(glm::max(tri.v0, tri.v1), tri.v2));
            rightBounds[i - start] = rightNode;
        }

        // 遍历所有可能的分割点，计算 SAH 代价
        for (int i = start; i < end - 1; ++i) {
            int leftCount = i - start + 1;
            int rightCount = end - i - 1;

            float leftArea = computeSurfaceArea(leftBounds[i - start]);
            float rightArea = computeSurfaceArea(rightBounds[i - start + 1]);
            float parentArea = computeSurfaceArea(leftBounds[end - start - 1]);

            float cost = leftArea / parentArea * leftCount + rightArea / parentArea * rightCount;

            if (cost < bestCost) {
                bestCost = cost;
                bestSplit = i + 1;
            }
        }
    }

    return bestSplit;
}

float PathTracingResourceManager::computeSurfaceArea(const BVHNode& node) {
    glm::vec3 size = node.maxBounds - node.minBounds;
    return 2.0f * (size.x * size.y + size.y * size.z + size.z * size.x);
}

void PathTracingResourceManager::createTriangleStorageBuffer(){
    VkDeviceSize bufferSize = sizeof(triangles[0]) * triangles.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VulkanUtils& vulkanUtils = VulkanUtils::getInstance();
    vulkanUtils.createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, triangles.data(), (size_t)bufferSize);

    vulkanUtils.createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, triangleStorageBuffer, triangleStorageBufferMemory);
    vulkanUtils.copyBuffer(device, commandManager->getCommandPool(), graphicsQueue, stagingBuffer, triangleStorageBuffer, bufferSize);

    memset(data, 0, bufferSize);
    bufferSize = sizeof(emissiveTriangles[0]) * emissiveTriangles.size();
    memcpy(data, emissiveTriangles.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    vulkanUtils.createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, emissiveTrianglesBuffer, emissiveTrianglesBufferMemory);
    vulkanUtils.copyBuffer(device, commandManager->getCommandPool(), graphicsQueue, stagingBuffer, emissiveTrianglesBuffer, bufferSize);

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

    if (cameraData.invViewProj != lastInvViewProj) {
        // 摄像机参数发生变化，重置采样计数
        totalSampleCount = 0;
        framesToForceZero = maxFramesInFlight;

        lastInvViewProj = cameraData.invViewProj;
    }

    if(framesToForceZero > 0){
        cameraData.frame = 0;
        framesToForceZero--; // 消耗一个“强制为零”的帧机会
    }else{
        cameraData.frame = totalSampleCount++;
    }

    memcpy(cameraDataBuffersMapped[currentFrame], &cameraData, sizeof(CameraData));
}