#pragma once
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE  
#define GLM_FORCE_DEPTH_ZERO_TO_ONE  
#endif  
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// 方向枚举，处理键盘输入
enum class CameraMovement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,    
    DOWN   
};

class Camera {
public:
    // Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch);
    void init() {
        position = glm::vec3(0.0f, 1.0f, 5.0f);
        worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
        yaw = -90.0f;
        pitch = 0.0f;
        updateCameraVectors();
    }
    glm::mat4 getViewMatrix() const;

    void processKeyboard(CameraMovement direction, float deltaTime);
    void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);

    glm::vec3 getPosition() const { return position; }
    glm::vec3 getFront() const { return front; }

private:
    void updateCameraVectors();

    // Camera Attributes
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    // Euler Angles
    float yaw;
    float pitch;

    // Options
    float movementSpeed = 2.5f;
    float mouseSensitivity = 0.1f;
};
