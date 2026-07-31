#include "vulkanContext.h"

static std::string EngineName = "CoolEngine";

void VulkanContext::init(Window& window) {

    vkb::InstanceBuilder builder;
    auto instanceResult = builder.set_app_name(window.getTitle().c_str())
        .set_engine_name(EngineName.c_str())
        .require_api_version(1, 3, 0)
        .enable_validation_layers(bUseValidationLayers)
        .build();

    if (!instanceResult) {
        throw std::runtime_error(instanceResult.error().message());
    }
    vkb::Instance vkbInstance = instanceResult.value();

    instance = vk::raii::Instance{ context, vkbInstance.instance };
    debugMessenger = vk::raii::DebugUtilsMessengerEXT{ instance, vkbInstance.debug_messenger};

    surface = window.createSurface(instance, context);

    auto devices = vk::raii::PhysicalDevices(instance);
    auto selectDevice = [&](bool discreteOnly) {
        for (const auto& candidate : devices) {
            if (discreteOnly &&
                candidate.getProperties().deviceType != vk::PhysicalDeviceType::eDiscreteGpu) {
                continue;
            }

            bool hasSwapchain = false;
            for (const auto& extension : candidate.enumerateDeviceExtensionProperties()) {
                if (std::string_view(extension.extensionName) == vk::KHRSwapchainExtensionName) {
                    hasSwapchain = true;
                    break;
                }
            }
            if (!hasSwapchain) {
                continue;
            }

            auto supportedFeatures =
                candidate.getFeatures2<vk::PhysicalDeviceFeatures2,
                                       vk::PhysicalDeviceVulkan13Features>();
            const auto& coreFeatures = supportedFeatures.get<vk::PhysicalDeviceFeatures2>().features;
            const auto& vulkan13Features = supportedFeatures.get<vk::PhysicalDeviceVulkan13Features>();
            if (!coreFeatures.samplerAnisotropy ||
                !vulkan13Features.dynamicRendering ||
                !vulkan13Features.synchronization2) {
                continue;
            }

            uint32_t graphics = ~0u;
            uint32_t present = ~0u;
            const auto queueFamilies = candidate.getQueueFamilyProperties();
            for (uint32_t index = 0; index < queueFamilies.size(); ++index) {
                if (graphics == ~0u &&
                    static_cast<bool>(queueFamilies[index].queueFlags &
                                      vk::QueueFlagBits::eGraphics)) {
                    graphics = index;
                }
                if (present == ~0u && candidate.getSurfaceSupportKHR(index, *surface)) {
                    present = index;
                }
            }

            if (graphics != ~0u && present != ~0u) {
                physicalDevice = candidate;
                graphicsQueueIndex = graphics;
                presentQueueIndex = present;
                return true;
            }
        }
        return false;
    };

    if (!selectDevice(true) && !selectDevice(false)) {
        throw std::runtime_error(
            "Could not find a Vulkan device with swapchain, graphics, presentation, "
            "dynamic rendering, synchronization2, and sampler anisotropy support");
    }


    constexpr float queuePriority = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> deviceQueueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { graphicsQueueIndex, presentQueueIndex };
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        deviceQueueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags(), queueFamily, 1, &queuePriority);
    }

    vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features>
        featureChain;
    featureChain.get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy = VK_TRUE;
    featureChain.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 = VK_TRUE;
    featureChain.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering = VK_TRUE;

    std::vector<const char*> requiredDeviceExtension = { vk::KHRSwapchainExtensionName };
    vk::DeviceCreateInfo deviceCreateInfo{};

    deviceCreateInfo.setQueueCreateInfos(deviceQueueCreateInfos)
        .setPNext(&featureChain.get<vk::PhysicalDeviceFeatures2>())
        .setEnabledExtensionCount(static_cast<uint32_t>(requiredDeviceExtension.size()))
        .setPEnabledExtensionNames(requiredDeviceExtension);

    device = physicalDevice.createDevice(deviceCreateInfo);

    graphicsQueue = device.getQueue(graphicsQueueIndex, 0);
    presentQueue = device.getQueue(presentQueueIndex, 0);

    vk::CommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo
        .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
        .setQueueFamilyIndex(graphicsQueueIndex);
        
    commandPool = device.createCommandPool(commandPoolCreateInfo);

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = *physicalDevice;
    allocatorInfo.device = *device;
    allocatorInfo.instance = *instance;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;

    VkResult result = vmaCreateAllocator(&allocatorInfo, &allocator);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create VMA allocator");
    }

};

