#include "swapchain.h"

void Swapchain::createImageViews() {
    swapChainImageViews.clear();
    for (const auto& image : swapchainImages) {
        vk::ImageSubresourceRange subresourceRange{};
        subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor)
            .setBaseMipLevel(0)
            .setLevelCount(1)
            .setBaseArrayLayer(0)
            .setLayerCount(1);

        vk::ComponentMapping components{};
        components.setR(vk::ComponentSwizzle::eIdentity)
            .setG(vk::ComponentSwizzle::eIdentity)
            .setB(vk::ComponentSwizzle::eIdentity)
            .setA(vk::ComponentSwizzle::eIdentity);

        vk::ImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.setImage(image)
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(swapchainImageFormat)
            .setComponents(components)
            .setSubresourceRange(subresourceRange);

        swapChainImageViews.emplace_back(vulkanContext.device.createImageView(imageViewCreateInfo));
    }
}


void Swapchain::create(Window& window)
{
    vk::SurfaceCapabilitiesKHR surfaceCapabilities = vulkanContext.physicalDevice.getSurfaceCapabilitiesKHR(*vulkanContext.surface);

    swapchainExtent = surfaceCapabilities.currentExtent; // will need later too
    if (!swapchainExtent.width || !swapchainExtent.height) {
        swapchainExtent.width = std::clamp(window.getWidth(), surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
        swapchainExtent.height = std::clamp(window.getHeight(), surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
    }

    uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, surfaceCapabilities.maxImageCount);
    }

    auto formats = vulkanContext.physicalDevice.getSurfaceFormatsKHR(*vulkanContext.surface);
    swapchainImageFormat = formats[0].format;
    swapchainColorSpace = formats[0].colorSpace;

    vk::SwapchainCreateInfoKHR swapchainCreateInfo{};
    swapchainCreateInfo.setSurface(*vulkanContext.surface)
        .setMinImageCount(imageCount)
        .setImageFormat(swapchainImageFormat)
        .setImageExtent(swapchainExtent)
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
        .setImageSharingMode(vk::SharingMode::eExclusive)
        .setPreTransform(surfaceCapabilities.currentTransform)
        .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
        .setPresentMode(vk::PresentModeKHR::eFifo)
        .setClipped(VK_TRUE)
        .setImageColorSpace(swapchainColorSpace);

    swapchain = vulkanContext.device.createSwapchainKHR(swapchainCreateInfo);
    swapchainImages = swapchain.getImages();
}

void Swapchain::recreate(Window& window) {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window.getGLFWWindow(), &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window.getGLFWWindow(), &width, &height);
        glfwWaitEvents();
    }

    vulkanContext.device.waitIdle();
	swapChainImageViews.clear();
	swapchain = nullptr;
	create(window);
	createImageViews();
}

Swapchain::Swapchain(VulkanContext& vulkanContext,Window& window) : vulkanContext(vulkanContext) {
	create(window);
	createImageViews();
}

