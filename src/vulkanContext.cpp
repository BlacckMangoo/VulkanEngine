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

    auto devices = vk::raii::PhysicalDevices(instance);
    for (const auto& device : devices) {
        if (device.getProperties().deviceType ==vk::PhysicalDeviceType::eDiscreteGpu) {
            physicalDevice = device;
            break;
        }
    };
    

    surface = window.createSurface(instance, context);
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
    for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size();qfpIndex++)
    {
        const bool supportsGraphics = static_cast<bool>(queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics);
        const bool supportsPresent = physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface) == VK_TRUE;

        if (supportsGraphics && graphicsQueueIndex == ~0u) graphicsQueueIndex = qfpIndex;
        if (supportsPresent && presentQueueIndex == ~0u) presentQueueIndex = qfpIndex;

        // early out once both are found
        if (graphicsQueueIndex != ~0u && presentQueueIndex != ~0u)
            break;
    }

    if (graphicsQueueIndex == ~0u || presentQueueIndex == ~0u) {
        throw std::runtime_error("Could not find suitable graphics/present queue families -> terminating");
    }


    constexpr float queuePriority = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> deviceQueueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { graphicsQueueIndex, presentQueueIndex };
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        deviceQueueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags(), queueFamily, 1, &queuePriority);
    }

    vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
        featureChain;
    featureChain.get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy = VK_TRUE;
    featureChain.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters = VK_TRUE;
    featureChain.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 = VK_TRUE;

    featureChain.get<vk::PhysicalDeviceVulkan12Features>().descriptorIndexing = VK_TRUE;
    featureChain.get<vk::PhysicalDeviceVulkan12Features>().shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    featureChain.get<vk::PhysicalDeviceVulkan12Features>().descriptorBindingVariableDescriptorCount = VK_TRUE;
    featureChain.get<vk::PhysicalDeviceVulkan12Features>().runtimeDescriptorArray = VK_TRUE;
    featureChain.get<vk::PhysicalDeviceVulkan12Features>().bufferDeviceAddress = VK_TRUE;
    featureChain.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering = VK_TRUE;

    featureChain.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState = VK_TRUE;

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

