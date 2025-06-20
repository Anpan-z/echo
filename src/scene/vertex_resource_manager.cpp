#include "vertex_resource_manager.hpp"
#include "vulkan_utils.hpp"
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <unordered_map>
#define TINYOBJLOADER_IMPLEMENTATION // 在一个源文件中定义 tinyobj_loader 的实现
#include <tiny_obj_loader.h>         // 包含 tinyobj_loader 库

void VertexResourceManager::init(VkDevice device, VkPhysicalDevice physicalDevice, CommandManager& commandManager)
{
    this->device = device;
    this->physicalDevice = physicalDevice;
    this->commandManager = &commandManager;

    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers(MAX_FRAMES_IN_FLIGHT);
    createSkyBoxVertexBuffer();
}

void VertexResourceManager::cleanup()
{
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);

    vkDestroyBuffer(device, indexBuffer, nullptr);
    vkFreeMemory(device, indexBufferMemory, nullptr);

    vkDestroyBuffer(device, skyBoxVertexBuffer, nullptr);
    vkFreeMemory(device, skyBoxVertexBufferMemory, nullptr);
    for (size_t i = 0; i < uniformBuffers.size(); i++)
    {
        vkDestroyBuffer(device, uniformBuffers[i], nullptr);
        vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
    }
    for (size_t i = 0; i < materialUniformBuffers.size(); i++)
    {
        vkDestroyBuffer(device, materialUniformBuffers[i], nullptr);
        vkFreeMemory(device, materialUniformBuffersMemory[i], nullptr);
    }
}

void VertexResourceManager::generateNormals(tinyobj::attrib_t& attrib, std::vector<tinyobj::shape_t>& shapes)
{
    size_t numVertices = attrib.vertices.size() / 3;
    std::vector<glm::vec3> vertexNormals(numVertices, glm::vec3(0.0f));

    for (const auto& shape : shapes)
    {
        size_t index_offset = 0;

        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++)
        {
            int fv = shape.mesh.num_face_vertices[f];

            if (fv != 3)
            {
                std::cerr << "Non-triangular face, skipping..." << std::endl;
                index_offset += fv;
                continue;
            }

            tinyobj::index_t idx0 = shape.mesh.indices[index_offset + 0];
            tinyobj::index_t idx1 = shape.mesh.indices[index_offset + 1];
            tinyobj::index_t idx2 = shape.mesh.indices[index_offset + 2];

            glm::vec3 v0(attrib.vertices[3 * idx0.vertex_index + 0], attrib.vertices[3 * idx0.vertex_index + 1],
                         attrib.vertices[3 * idx0.vertex_index + 2]);

            glm::vec3 v1(attrib.vertices[3 * idx1.vertex_index + 0], attrib.vertices[3 * idx1.vertex_index + 1],
                         attrib.vertices[3 * idx1.vertex_index + 2]);

            glm::vec3 v2(attrib.vertices[3 * idx2.vertex_index + 0], attrib.vertices[3 * idx2.vertex_index + 1],
                         attrib.vertices[3 * idx2.vertex_index + 2]);

            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;
            glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));

            vertexNormals[idx0.vertex_index] = faceNormal;
            vertexNormals[idx1.vertex_index] = faceNormal;
            vertexNormals[idx2.vertex_index] = faceNormal;

            index_offset += fv;
        }
    }

    // 清空已有法线（如果有）
    attrib.normals.clear();

    // 归一化并写入 attrib.normals
    for (size_t i = 0; i < vertexNormals.size(); ++i)
    {
        glm::vec3 n = glm::normalize(vertexNormals[i]);
        attrib.normals.push_back(n.x);
        attrib.normals.push_back(n.y);
        attrib.normals.push_back(n.z);
    }

    for (auto& shape : shapes)
    {
        for (auto& index : shape.mesh.indices)
        {
            index.normal_index = index.vertex_index;
        }
    }

    std::cout << "Generated smooth normals for " << numVertices << " vertices." << std::endl;
}

void VertexResourceManager::loadModel(const std::string& modelPath, const std::string& materialPath)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str(), materialPath.c_str()))
    {
        throw std::runtime_error(warn + err);
    }
    if (attrib.normals.empty())
    {
        std::cout << "Model has no normal data, generating normals..." << std::endl;
        generateNormals(attrib, shapes);
    }
    else
    {
        std::cout << "Model already has normal data." << std::endl;
    }
    // GenerateNormals(attrib, shapes);
    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    size_t shapeIndex = 0;
    for (const auto& shape : shapes)
    {
        size_t indexOffset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++)
        {
            int fv = shape.mesh.num_face_vertices[f];
            int matId = shape.mesh.material_ids[f];

            glm::vec3 color(1.0f);
            if (matId >= 0 && matId < materials.size())
            {
                const auto& mat = materials[matId];
                // 使用材质的漫反射颜色
                color = glm::vec3(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]);
            }

            for (size_t v = 0; v < fv; v++)
            {
                tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];
                Vertex vertex{};

                // 顶点坐标
                vertex.pos = {attrib.vertices[3 * idx.vertex_index + 0], attrib.vertices[3 * idx.vertex_index + 1],
                              attrib.vertices[3 * idx.vertex_index + 2]};

                // 法线（可能不存在）
                if (idx.normal_index >= 0)
                {
                    vertex.normal = {attrib.normals[3 * idx.normal_index + 0], attrib.normals[3 * idx.normal_index + 1],
                                     attrib.normals[3 * idx.normal_index + 2]};
                }
                else
                {
                    vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f); // 默认朝上，或你后面再计算 face normal
                }

                vertex.color = color;
                vertex.materialID = shapeIndex; // 设置材质 ID
                if (uniqueVertices.count(vertex) == 0)
                {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertex]);
            }
            indexOffset += fv;
        }
        shapeIndex += 1;
        float emission = 0.0f;
        if (shape.name == "light")
        {
            emission = 20.0f; // 如果是光源，设置自发光强度
        }
        materialUniformBufferObjects.push_back(
            std::make_shared<MaterialUniformBufferObject>(vertices.back().color, emission)); // 添加材质统一缓冲区对象
        preMaterialUniformBufferObjects.push_back(
            std::make_shared<MaterialUniformBufferObject>(vertices.back().color, emission));
        shapeNames.push_back(shape.name); // 存储形状名称
    }
}

void VertexResourceManager::reloadModel(const std::string& modelPath, const std::string& materialPath)
{
    vkDeviceWaitIdle(device);
    vertices.clear();
    indices.clear();
    shapeNames.clear();
    materialUniformBufferObjects.clear();
    cleanup(); // 清理之前的资源
    // vkDestroyBuffer(device, vertexBuffer, nullptr);
    // vkFreeMemory(device, vertexBufferMemory, nullptr);

    // vkDestroyBuffer(device, indexBuffer, nullptr);
    // vkFreeMemory(device, indexBufferMemory, nullptr);

    loadModel(modelPath, materialPath);

    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers(MAX_FRAMES_IN_FLIGHT); // 重新创建统一缓冲区

    createSkyBoxVertexBuffer();

    for (auto observer : modelReloadObservers)
    {
        observer->onModelReloaded();
    }
}

void VertexResourceManager::createVertexBuffer()
{
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
                 stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

    copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VertexResourceManager::createIndexBuffer()
{
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
                 stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

    copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VertexResourceManager::createSkyBoxVertexBuffer()
{
    // 创建天空盒顶点缓冲区
    std::vector<Vertex> skyboxVertices = {
        // +X (right)
        {{1.0f, 1.0f, -1.0f}, {}, {1.0f, 0.0f, 0.0f}, 0},
        {{1.0f, -1.0f, -1.0f}, {}, {1.0f, 0.0f, 0.0f}, 0},
        {{1.0f, -1.0f, 1.0f}, {}, {1.0f, 0.0f, 0.0f}, 0},
        {{1.0f, 1.0f, -1.0f}, {}, {1.0f, 0.0f, 0.0f}, 0},
        {{1.0f, -1.0f, 1.0f}, {}, {1.0f, 0.0f, 0.0f}, 0},
        {{1.0f, 1.0f, 1.0f}, {}, {1.0f, 0.0f, 0.0f}, 0},

        // -X (left)
        {{-1.0f, 1.0f, 1.0f}, {}, {-1.0f, 0.0f, 0.0f}, 0},
        {{-1.0f, -1.0f, 1.0f}, {}, {-1.0f, 0.0f, 0.0f}, 0},
        {{-1.0f, -1.0f, -1.0f}, {}, {-1.0f, 0.0f, 0.0f}, 0},
        {{-1.0f, 1.0f, 1.0f}, {}, {-1.0f, 0.0f, 0.0f}, 0},
        {{-1.0f, -1.0f, -1.0f}, {}, {-1.0f, 0.0f, 0.0f}, 0},
        {{-1.0f, 1.0f, -1.0f}, {}, {-1.0f, 0.0f, 0.0f}, 0},

        // +Y (top)
        {{-1.0f, 1.0f, 1.0f}, {}, {0.0f, 1.0f, 0.0f}, 0},
        {{-1.0f, 1.0f, -1.0f}, {}, {0.0f, 1.0f, 0.0f}, 0},
        {{1.0f, 1.0f, -1.0f}, {}, {0.0f, 1.0f, 0.0f}, 0},
        {{-1.0f, 1.0f, 1.0f}, {}, {0.0f, 1.0f, 0.0f}, 0},
        {{1.0f, 1.0f, -1.0f}, {}, {0.0f, 1.0f, 0.0f}, 0},
        {{1.0f, 1.0f, 1.0f}, {}, {0.0f, 1.0f, 0.0f}, 0},

        // -Y (bottom)
        {{-1.0f, -1.0f, -1.0f}, {}, {0.0f, -1.0f, 0.0f}, 0},
        {{-1.0f, -1.0f, 1.0f}, {}, {0.0f, -1.0f, 0.0f}, 0},
        {{1.0f, -1.0f, 1.0f}, {}, {0.0f, -1.0f, 0.0f}, 0},
        {{-1.0f, -1.0f, -1.0f}, {}, {0.0f, -1.0f, 0.0f}, 0},
        {{1.0f, -1.0f, 1.0f}, {}, {0.0f, -1.0f, 0.0f}, 0},
        {{1.0f, -1.0f, -1.0f}, {}, {0.0f, -1.0f, 0.0f}, 0},

        // +Z (front)
        {{1.0f, 1.0f, 1.0f}, {}, {0.0f, 0.0f, -1.0f}, 0},
        {{1.0f, -1.0f, 1.0f}, {}, {0.0f, 0.0f, -1.0f}, 0},
        {{-1.0f, -1.0f, 1.0f}, {}, {0.0f, 0.0f, -1.0f}, 0},
        {{1.0f, 1.0f, 1.0f}, {}, {0.0f, 0.0f, -1.0f}, 0},
        {{-1.0f, -1.0f, 1.0f}, {}, {0.0f, 0.0f, -1.0f}, 0},
        {{-1.0f, 1.0f, 1.0f}, {}, {0.0f, 0.0f, -1.0f}, 0},

        // -Z (back)
        {{-1.0f, 1.0f, -1.0f}, {}, {0.0f, 0.0f, 1.0f}, 0},
        {{-1.0f, -1.0f, -1.0f}, {}, {0.0f, 0.0f, 1.0f}, 0},
        {{1.0f, -1.0f, -1.0f}, {}, {0.0f, 0.0f, 1.0f}, 0},
        {{-1.0f, 1.0f, -1.0f}, {}, {0.0f, 0.0f, 1.0f}, 0},
        {{1.0f, -1.0f, -1.0f}, {}, {0.0f, 0.0f, 1.0f}, 0},
        {{1.0f, 1.0f, -1.0f}, {}, {0.0f, 0.0f, 1.0f}, 0},
    };
    VkDeviceSize bufferSize = sizeof(skyboxVertices[0]) * skyboxVertices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
                 stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, skyboxVertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, skyBoxVertexBuffer, skyBoxVertexBufferMemory);

    copyBuffer(stagingBuffer, skyBoxVertexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VertexResourceManager::createUniformBuffers(size_t maxFramesInFlight)
{
    VkDeviceSize bufferSize = sizeof(UniformBufferObject); // 假设统一缓冲区存储矩阵

    uniformBuffers.resize(maxFramesInFlight);
    uniformBuffersMemory.resize(maxFramesInFlight);
    uniformBuffersMapped.resize(maxFramesInFlight);

    for (size_t i = 0; i < maxFramesInFlight; i++)
    {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i],
                     uniformBuffersMemory[i]);
        vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }

    if (shapeNames.empty())
    {
        std::cerr << "Error: No shapes loaded. Cannot create material uniform buffers." << std::endl;
        return;
    }
    // 创建材质统一缓冲区
    bufferSize = sizeof(MaterialUniformBufferObject) * shapeNames.size();
    materialUniformBuffers.resize(maxFramesInFlight);
    materialUniformBuffersMemory.resize(maxFramesInFlight);
    materialUniformBuffersMapped.resize(maxFramesInFlight);
    for (size_t i = 0; i < maxFramesInFlight; i++)
    {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     materialUniformBuffers[i], materialUniformBuffersMemory[i]);
        vkMapMemory(device, materialUniformBuffersMemory[i], 0, bufferSize, 0, &materialUniformBuffersMapped[i]);
    }
}

void VertexResourceManager::updateUniformBuffer(uint32_t currentFrame, VkExtent2D swapChainExtent, Camera& camera)
{
    // static auto startTime = std::chrono::high_resolution_clock::now();

    // auto currentTime = std::chrono::high_resolution_clock::now();
    // float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    // ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.model = glm::mat4(1.0f);
    // ubo.view = glm::lookAt(glm::vec3(0.0f, -5.0f, 0.5f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(0.0f, 1.0f, 5.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.proj =
        glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);

    ubo.proj[1][1] *= -1;
    ubo.view = camera.getViewMatrix();    // 使用相机的视图矩阵
    ubo.cameraPos = camera.getPosition(); // 使用相机的位置
    // ubo.cameraPos = glm::vec3(0.0f, 1.0f, 5.0f);

    auto lightPos = glm::vec3(3.0f, 3.0f, 3.0f);
    glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    // glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 20.0f);
    glm::mat4 lightProjection = glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, 1.0f, 10.0f);
    lightProjection[1][1] *= -1; // 反转Y轴
    ubo.lightSpaceMatrix = lightProjection * lightView * glm::mat4(1.0f);

    memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));

    std::vector<MaterialUniformBufferObject> materialUbo(shapeNames.size());

    for (size_t i = 0; i < shapeNames.size(); i++)
    {
        materialUbo[i].albedo = materialUniformBufferObjects[i]->albedo;
        materialUbo[i].metallic = materialUniformBufferObjects[i]->metallic;
        materialUbo[i].roughness = materialUniformBufferObjects[i]->roughness;
        materialUbo[i].ambientOcclusion = materialUniformBufferObjects[i]->ambientOcclusion;
        materialUbo[i].emission = materialUniformBufferObjects[i]->emission;
    }
    memcpy(materialUniformBuffersMapped[currentFrame], materialUbo.data(),
           sizeof(MaterialUniformBufferObject) * shapeNames.size());

    // 检查材质是否发生变化，并通知观察者
    bool materialChanged = false;
    for (size_t i = 0; i < shapeNames.size(); i++)
    {
        // 更新材质统一缓冲区对象
        if (preMaterialUniformBufferObjects[i]->albedo != materialUbo[i].albedo)
            materialChanged = true;
        if (preMaterialUniformBufferObjects[i]->metallic != materialUbo[i].metallic)
            materialChanged = true;
        if (preMaterialUniformBufferObjects[i]->roughness != materialUbo[i].roughness)
            materialChanged = true;
        if (preMaterialUniformBufferObjects[i]->ambientOcclusion != materialUbo[i].ambientOcclusion)
            materialChanged = true;
    }
    if (materialChanged)
    {
        for (size_t i = 0; i < shapeNames.size(); i++)
        {
            preMaterialUniformBufferObjects[i]->albedo = materialUbo[i].albedo;
            preMaterialUniformBufferObjects[i]->metallic = materialUbo[i].metallic;
            preMaterialUniformBufferObjects[i]->roughness = materialUbo[i].roughness;
            preMaterialUniformBufferObjects[i]->ambientOcclusion = materialUbo[i].ambientOcclusion;
        }
        for (auto observer : materialUpdateObservers)
        {
            observer->onMaterialUpdated();
        }
    }
}

void VertexResourceManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                                         VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanUtils::getInstance().findMemoryType(
        physicalDevice, memRequirements.memoryTypeBits, properties); // 需要实现 findMemoryType 函数

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void VertexResourceManager::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer = commandManager->beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0; // Optional
    copyRegion.dstOffset = 0; // Optional
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    commandManager->endSingleTimeCommands(commandBuffer);
}