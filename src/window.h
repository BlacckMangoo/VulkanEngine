#pragma once 
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <memory>
#include "common.h"
#include "input.h"


class Window {

public:
  Window(int width, int height, const std::string &title);
  ~Window();
  GLFWwindow *getGLFWWindow() const { return window; }
  uint32_t getWidth() const { return width; }
  uint32_t getHeight() const { return height; }
  const std::string& getTitle() const { return title; }
  vk::raii::SurfaceKHR createSurface(const vk::raii::Instance &instance,const vk::raii::Context &context) const;
  bool shouldClose() const { return glfwWindowShouldClose(window); }
  InputState& getInputState() { return inputState; }
  const InputState& getInputState() const { return inputState; }
private:
  GLFWwindow *window;
  uint32_t width;
  uint32_t height;
  std::string title;
  InputState inputState;
  std::unique_ptr<GlfwInputAdapter> inputAdapter;
};
