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
#include "vertex.hpp" // 包含顶点结构体定义
#include "command_manager.hpp"
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

class ResourceManager {
public:
    // static ResourceManager& getInstance();

    // ResourceManager(const ResourceManager&) = delete;
    // ResourceManager& operator=(const ResourceManager&) = delete;


    void init(VkDevice device, VkPhysicalDevice physicalDevice, CommandManager& commandManager);
    // ~ResourceManager();
    void cleanup();

    void loadModel(const std::string& modelPath, const std::string& materialPath);

    void createVertexBuffer();

    void createIndexBuffer();

    void createUniformBuffers(size_t maxFramesInFlight);

    void updateUniformBuffer(uint32_t currentFrame, VkExtent2D swapChainExtent);

    const std::vector<Vertex>& getVertices() const{return vertices;};

    const std::vector<uint32_t>& getIndices() const{return indices;};

    const std::vector<VkBuffer>& getUniformBuffers() const{return uniformBuffers;};

    const std::vector<VkDeviceMemory>& getUniformBuffersMemory() const{return uniformBuffersMemory;};

    const std::vector<void*>& getUniformBuffersMapped() const{return uniformBuffersMapped;};
    
    VkBuffer getVertexBuffer() const { return vertexBuffer; }

    VkBuffer getIndexBuffer() const { return indexBuffer; }


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

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    // 工具函数
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void generateNormals(tinyobj::attrib_t& attrib, std::vector<tinyobj::shape_t>& shapes);

    // ResourceManager() = default;
};