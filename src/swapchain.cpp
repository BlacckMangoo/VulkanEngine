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

    if (surfaceCapabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
        swapchainExtent = surfaceCapabilities.currentExtent;
    } else {
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window.getGLFWWindow(), &framebufferWidth, &framebufferHeight);
        swapchainExtent.width = std::clamp(
            static_cast<uint32_t>(framebufferWidth),
            surfaceCapabilities.minImageExtent.width,
            surfaceCapabilities.maxImageExtent.width);
        swapchainExtent.height = std::clamp(
            static_cast<uint32_t>(framebufferHeight),
            surfaceCapabilities.minImageExtent.height,
            surfaceCapabilities.maxImageExtent.height);
    }

    uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, surfaceCapabilities.maxImageCount);
    }

    auto formats = vulkanContext.physicalDevice.getSurfaceFormatsKHR(*vulkanContext.surface);
    if (formats.empty()) {
        throw std::runtime_error("Surface exposes no swapchain formats");
    }
    swapchainImageFormat = formats[0].format;
    swapchainColorSpace = formats[0].colorSpace;

    vk::SwapchainCreateInfoKHR swapchainCreateInfo{};
    swapchainCreateInfo.setSurface(*vulkanContext.surface)
        .setMinImageCount(imageCount)
        .setImageFormat(swapchainImageFormat)
        .setImageExtent(swapchainExtent)
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
        .setPreTransform(surfaceCapabilities.currentTransform)
        .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
        .setPresentMode(vk::PresentModeKHR::eFifo)
        .setClipped(VK_TRUE)
        .setImageColorSpace(swapchainColorSpace);

    const std::array queueFamilyIndices{
        vulkanContext.graphicsQueueIndex, vulkanContext.presentQueueIndex};
    if (vulkanContext.graphicsQueueIndex != vulkanContext.presentQueueIndex) {
        swapchainCreateInfo
            .setImageSharingMode(vk::SharingMode::eConcurrent)
            .setQueueFamilyIndices(queueFamilyIndices);
    } else {
        swapchainCreateInfo.setImageSharingMode(vk::SharingMode::eExclusive);
    }

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

