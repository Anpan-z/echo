#pragma once

#include "vertex.hpp"
#include "command_manager.hpp"
#include "vertex_resource_manager.hpp"
#include "swap_chain_manager.hpp"
#include <vulkan/vulkan.h>  
#include <glm/glm.hpp>

struct Triangle {
    alignas(16) glm::vec3 v0, v1, v2; // 三角形的三个顶点
    alignas(16) glm::vec3 normal;     // 三角形的法线
    alignas(4) uint32_t materialID; // 三角形的材质 ID
};

struct CameraData {
    glm::mat4 invViewProj; // 逆投影矩阵
    glm::vec3 cameraPos;   // 摄像机位置
    int frame;             // 当前帧编号
};

class PathTracingResourceManager {
public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue graphicsQueue, SwapChainManager& swapChainManager, CommandManager& commandManager, VertexResourceManager& vertexResourceManager);

    void cleanup();

    void updateCameraDataBuffer(uint32_t currentFrame, VkExtent2D swapChainExtent, Camera& camera);

    void recreatePathTracingOutputImages();
    
    std::vector<VkImage> getPathTracingOutputImages() const { return storageImages; }
    
    std::vector<VkImageView> getPathTracingOutputImageviews() const { return storageImageViews; }

    VkBuffer getStorageBuffer() const { return storageBuffer; }

    VkExtent2D getOutputExtent() const { return outPutExtent; }

    std::vector<VkBuffer> getMaterialUniformBuffers() const { return *materialUniformBuffers; }
    
    const std::vector<std::string>& getShapeNames() const { return vertexResourceManager->getShapeNames(); }

    const std::vector<VkBuffer>& getCameraDataBuffer() const { return cameraDataBuffer; }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    SwapChainManager* swapChainManager = nullptr;
    VkExtent2D outPutExtent;
    CommandManager* commandManager = nullptr;
    VertexResourceManager* vertexResourceManager = nullptr;

    std::vector<Triangle> triangles;
    std::vector<VkBuffer>* materialUniformBuffers;

    VkBuffer storageBuffer;
    VkDeviceMemory storageBufferMemory;

    std::vector<VkImage> storageImages;
    std::vector<VkDeviceMemory> storageImageMemories;
    std::vector<VkImageView> storageImageViews;

    std::vector<VkBuffer> cameraDataBuffer;
    std::vector<VkDeviceMemory> cameraDataBufferMemory;
    std::vector<void*> cameraDataBuffersMapped;

    uint32_t totalSampleCount = 0;

    void buildTrianglesFromMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void createStorageBuffer();
    void createPathTracingOutputImages();

    void createCameraDataBuffer();
};