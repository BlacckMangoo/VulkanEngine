#include "window.h"

Window::Window(int width, int height, const std::string &title)
    : width(width), height(height), title(title){
  if (!glfwInit()) {
    throw std::runtime_error("Failed to initialize GLFW");
  }
  if (!glfwVulkanSupported()) {
    glfwTerminate();
    throw std::runtime_error("Vulkan support not available");
  }
  glfwWindowHint(GLFW_CLIENT_API,GLFW_NO_API); // by default open gl window ,need vulkan

  window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    throw std::runtime_error("Failed to create GLFW window");
  }

  inputAdapter = std::make_unique<GlfwInputAdapter>(window, inputState);

  // set input call backs 

  glfwSetKeyCallback(window, keyCallback);
  glfwSetCursorPosCallback(window, mouseMoveCallback);
  glfwSetMouseButtonCallback(window, mouseButtonCallback);
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
