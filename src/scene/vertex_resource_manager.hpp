#pragma once

#include <vulkan/vulkan.h>
#define GLM_ENABLE_EXPERIMENTAL
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE  
#define GLM_FORCE_DEPTH_ZERO_TO_ONE  
#endif 
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include "vertex.hpp" // 包含顶点结构体定义
#include "command_manager.hpp"
#include "camera.hpp"
#include <tiny_obj_loader.h> // 包含 tinyobj_loader 库

const int MAX_FRAMES_IN_FLIGHT = 2;

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec3 cameraPos; // 
    float padding;       // 保持对齐（vec3 + float = 16字节）
    glm::mat4 lightSpaceMatrix;
};

struct MaterialUniformBufferObject {
    alignas(16) glm::vec3 albedo;
    alignas(4)  float metallic;
    alignas(4)  float roughness;
    alignas(4)  float ambientOcclusion;
    alignas(4)  float padding2;  // 对齐
    alignas(4)  float emission; // 自发光强度

    MaterialUniformBufferObject(
        glm::vec3 albedo = glm::vec3(1.0f),
        float emission = 0.0f
    )
        : albedo(albedo),
          metallic(0.0f),
          roughness(0.5f),
          ambientOcclusion(1.0f),
          padding2(0.0f),
          emission(emission)
    {}
};

class ModelReloadObserver {
    public:
        virtual void onModelReloaded() = 0; // 当模型重新加载时的回调
        virtual ~ModelReloadObserver() = default;
    };

class MaterialIpdateObsever {
    public:
        virtual void onMaterialUpdated() = 0; // 当材质更新时的回调
        virtual ~MaterialIpdateObsever() = default;
    };

class VertexResourceManager {
public:
    // static VertexResourceManager& getInstance();

    // VertexResourceManager(const VertexResourceManager&) = delete;
    // VertexResourceManager& operator=(const VertexResourceManager&) = delete;

    void init(VkDevice device, VkPhysicalDevice physicalDevice, CommandManager& commandManager);
    // ~VertexResourceManager();
    void cleanup();

    void loadModel(const std::string& modelPath, const std::string& materialPath);

    void reloadModel(const std::string& modelPath, const std::string& materialPath);

    void createVertexBuffer();

    void createIndexBuffer();

    void createUniformBuffers(size_t maxFramesInFlight);

    void updateUniformBuffer(uint32_t currentFrame, VkExtent2D swapChainExtent, Camera& camera);

    void createSkyBoxVertexBuffer();

    const std::vector<Vertex>& getVertices() const{return vertices;};

    const std::vector<uint32_t>& getIndices() const{return indices;};

    const std::vector<VkBuffer>& getUniformBuffers() const{return uniformBuffers;};

    const std::vector<VkDeviceMemory>& getUniformBuffersMemory() const{return uniformBuffersMemory;};

    const std::vector<void*>& getUniformBuffersMapped() const{return uniformBuffersMapped;};

    std::vector<VkBuffer>& getMaterialUniformBuffers() {return materialUniformBuffers;};

    const std::vector<std::string>& getShapeNames() const { return shapeNames; }

    const std::vector<std::shared_ptr<MaterialUniformBufferObject>>& getMaterialUniformBufferObjects() const { return materialUniformBufferObjects; }

    float* getMetallics() const { return metallics; }
    
    VkBuffer getVertexBuffer() const { return vertexBuffer; }

    VkBuffer getIndexBuffer() const { return indexBuffer; }

    VkBuffer getSkyBoxVertexBuffer() const { return skyBoxVertexBuffer; }

    void addModelReloadObserver(ModelReloadObserver* observer) { modelReloadObservers.push_back(observer);}

    void addMaterialUpdateObserver(MaterialIpdateObsever* observer) { materialUpdateObservers.push_back(observer); }

private:
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    CommandManager* commandManager;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;

    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    VkBuffer skyBoxVertexBuffer;
    VkDeviceMemory skyBoxVertexBufferMemory;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    std::vector<VkBuffer> materialUniformBuffers;
    std::vector<VkDeviceMemory> materialUniformBuffersMemory;
    std::vector<void*> materialUniformBuffersMapped;

    // std::unordered_map<std::string, tinyobj::material_t> materials; // 材质映射
    // std::unordered_map<std::string, tinyobj::shape_t> shapes; // 形状映射
    // std::unordered_map<std::string, tinyobj::attrib_t> attribs; // 属性映射
    
    float metallicValue = 0.5f;
    float* metallics = &metallicValue; // 金属度数组
    std::vector<std::string> shapeNames; // 形状名称数组
    std::vector<std::shared_ptr<MaterialUniformBufferObject>> materialUniformBufferObjects; // 材质统一缓冲区对象
    std::vector<std::shared_ptr<MaterialUniformBufferObject>> preMaterialUniformBufferObjects; // 材质统一缓冲区对象
    
    std::vector<ModelReloadObserver*> modelReloadObservers; // 存储观察者
    std::vector<MaterialIpdateObsever*> materialUpdateObservers; // 存储材质更新观察者
    
    // 工具函数
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void generateNormals(tinyobj::attrib_t& attrib, std::vector<tinyobj::shape_t>& shapes);

    // VertexResourceManager() = default;
};