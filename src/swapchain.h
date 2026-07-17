#pragma once 
#include "vulkanContext.h"
#include "common.h"

class Swapchain {
public:
	Swapchain(VulkanContext& vulkanContext, Window& window);

	auto getExtent() const { return swapchainExtent; }
	vk::Format getFormat() const { return swapchainImageFormat; }
	auto getImagesCount() const { return swapchainImages.size(); }
	auto& get() const { return *swapchain; }
	auto& getImageViews() const { return swapChainImageViews; }	
	auto& getImages() const { return swapchainImages; }

	void createImageViews();
	void create( Window& window);
	void recreate( Window& window);

private:
	VulkanContext& vulkanContext;
	vk::Extent2D swapchainExtent;
	vk::Format swapchainImageFormat;
	vk::ColorSpaceKHR swapchainColorSpace;
	std::vector<vk::Image> swapchainImages;
	std::vector<vk::raii::ImageView> swapChainImageViews;
	vk::raii::SwapchainKHR swapchain{nullptr};

};