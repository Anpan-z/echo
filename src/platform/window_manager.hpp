#pragma once

#include <GLFW/glfw3.h>
#include <stdexcept>
#include <functional>

class WindowManager {
public:
    void init(uint32_t width, uint32_t height, const char* title);
    void cleanup();

    GLFWwindow* getWindow() const { return window; }
    bool shouldClose() const { return glfwWindowShouldClose(window); }
    void pollEvents() const { glfwPollEvents(); }
    void setFramebufferResizeCallback(GLFWframebuffersizefun callback);
    void setFramebufferResized(bool flag) { framebufferResized = flag; }
    bool isFramebufferResized() const { return framebufferResized; }

private:
    GLFWwindow* window = nullptr;
    std::function<void(int, int)> framebufferResizeCallback;
    bool framebufferResized = false;

    static void glfwFramebufferResizeCallback(GLFWwindow* window, int width, int height);
};