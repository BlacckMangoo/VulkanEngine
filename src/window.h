#pragma once 
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "common.h"

class Window {

public:
  Window(int width, int height, const std::string &title);
  ~Window();
  GLFWwindow *getGLFWWindow() const { return window; }
  uint32_t getWidth() const { return width; }
  uint32_t getHeight() const { return height; }
  auto getTitle() const { return title; };
  vk::raii::SurfaceKHR createSurface(const vk::raii::Instance &instance,const vk::raii::Context &context) const;
  bool shouldClose() const { return glfwWindowShouldClose(window); }

  bool framebufferResized = false;
private:
  void setInputCallbacks();
  GLFWwindow *window;
  uint32_t width;
  uint32_t height;
  VkSurfaceKHR surface; 
  const std::string& title;
};

static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    self->framebufferResized = true;
	std::cout << "Framebuffer resized: width=" << width << ", height=" << height << std::endl;
}

static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
	std::cout << "Scroll offset: x=" << xoffset << ", y=" << yoffset << std::endl;
}
