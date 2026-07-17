#pragma once 
#include "window.h"
#include <VkBootstrap.h>


#ifdef NDEBUG
constexpr bool bUseValidationLayers = false;
#else
constexpr bool bUseValidationLayers = true;
#endif

class VulkanContext
{
public:
    vk::raii::Context context{};

    vk::raii::Instance instance{ VK_NULL_HANDLE };
    vk::raii::PhysicalDevice physicalDevice{ VK_NULL_HANDLE };
    vk::raii::Device device{ VK_NULL_HANDLE };
    vk::raii::DebugUtilsMessengerEXT debugMessenger{ nullptr };
    vk::raii::Queue graphicsQueue{ VK_NULL_HANDLE };
    vk::raii::Queue presentQueue{ VK_NULL_HANDLE };

    uint32_t graphicsQueueIndex = ~0u;
    uint32_t presentQueueIndex = ~0u;

    vk::raii::CommandPool commandPool{ VK_NULL_HANDLE };
    vk::raii::SurfaceKHR surface{ VK_NULL_HANDLE };

    VmaAllocator allocator{};

    VulkanContext(Window& window) {
        init(window);
    }

    ~VulkanContext() {
        if (allocator) {
            vmaDestroyAllocator(allocator);
        }
    }
private:
    void init(Window& window);
};