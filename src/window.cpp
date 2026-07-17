#include "window.h"

void Window::setInputCallbacks() {
	glfwSetScrollCallback(window, scrollCallback);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

Window::Window(int width, int height, const std::string &title)
    : width(width), height(height), title(title) {
  if (!glfwInit()) {
    std::cerr << "Failed to initialize !" << std::endl;
  }
  if (!glfwVulkanSupported()) {
    glfwTerminate();
    std::cerr << "Vulkan support not available!" << std::endl;
  }
  glfwWindowHint(GLFW_CLIENT_API,
                 GLFW_NO_API); // by default open gl window ,need vulkan

  window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window!" << std::endl;
  }
 
  setInputCallbacks();
}

vk::raii::SurfaceKHR Window::createSurface(const vk::raii::Instance &instance,
                      const vk::raii::Context &context) const {
  VkSurfaceKHR surface;
  if (glfwCreateWindowSurface(*instance, window, nullptr, &surface) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create window surface!");
  }
  return vk::raii::SurfaceKHR(instance, surface);
}

Window::~Window() {
  if (window) {
    glfwDestroyWindow(window);
  }
  glfwTerminate();
}