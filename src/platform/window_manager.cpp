#include "window_manager.hpp"

void WindowManager::init(uint32_t width, uint32_t height, const char* title) {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW!");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window!");
    }

    // 设置 GLFW 的帧缓冲区大小调整回调
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, glfwFramebufferResizeCallback);
}

void WindowManager::cleanup() {
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    glfwTerminate();
}

void WindowManager::setFramebufferResizeCallback(GLFWframebuffersizefun callback) {
    glfwSetFramebufferSizeCallback(window, callback);
}

void WindowManager::glfwFramebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto manager = reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));
    if(manager){
        manager->framebufferResized = true;
    }
    // if (manager && manager->framebufferResizeCallback) {
    //     manager->framebufferResizeCallback(width, height);
    // }
}