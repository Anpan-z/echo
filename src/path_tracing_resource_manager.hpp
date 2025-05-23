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

class PathTracingResourceReloadObserver {
public:
    virtual void onModelReloaded() = 0; // 当模型重新加载时的回调
    virtual void onPathTracingOutputImagesRecreated() = 0; // 当路径追踪输出图像重新创建时的回调
    virtual ~PathTracingResourceReloadObserver() = default;
};

class PathTracingResourceManagerModelObserver; // 前向声明

class PathTracingResourceManager {
public:
    void init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue graphicsQueue, SwapChainManager& swapChainManager, CommandManager& commandManager, VertexResourceManager& vertexResourceManager);

    void cleanup();

    void updateCameraDataBuffer(uint32_t currentFrame, VkExtent2D swapChainExtent, Camera& camera);

    void recreatePathTracingOutputImages();

    void recreteTriangleData();
    
    std::vector<VkImage> getPathTracingOutputImages() const { return storageImages; }
    
    std::vector<VkImageView> getPathTracingOutputImageviews() const { return storageImageViews; }

    VkBuffer getStorageBuffer() const { return storageBuffer; }

    VkExtent2D getOutputExtent() const { return outPutExtent; }

    std::vector<VkBuffer> getMaterialUniformBuffers() const { return *materialUniformBuffers; }
    
    const std::vector<std::string>& getShapeNames() const { return vertexResourceManager->getShapeNames(); }

    const std::vector<VkBuffer>& getCameraDataBuffer() const { return cameraDataBuffer; }

    void addPathTracingResourceReloadObserver(PathTracingResourceReloadObserver* observer) {
        pathTracingResourceReloadObservers.push_back(observer);
    }

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

    std::unique_ptr<PathTracingResourceManagerModelObserver> pathTracingResourceManagerModelObserver;

    std::vector<PathTracingResourceReloadObserver*> pathTracingResourceReloadObservers;

    uint32_t totalSampleCount = 0;
    //用于rebuild相关资源时双帧或多帧同步
    uint32_t framesToForceZero = 0;
    uint32_t maxFramesInFlight;

    void buildTrianglesFromMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void createStorageBuffer();
    void createPathTracingOutputImages();

    void createCameraDataBuffer();
};

class PathTracingResourceManagerModelObserver : public ModelReloadObserver {
    public:
        PathTracingResourceManagerModelObserver(PathTracingResourceManager* pathTracingResourceManager) // 使用指向 RenderPipeline 的指针
            : pathTracingResourceManager(pathTracingResourceManager) {}
    
        void onModelReloaded() override {
            // 当模型重新加载时，更新 RenderPipeline 的描述符集
            if (pathTracingResourceManager) {
                pathTracingResourceManager->recreteTriangleData();
            }
        }
    
    private:
        PathTracingResourceManager* pathTracingResourceManager; // 持有 RenderPipeline 的指针
};