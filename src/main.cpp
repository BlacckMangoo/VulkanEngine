#include "window.h"
#include <fastgltf/core.hpp>
#include "vulkanContext.h"
#include "swapchain.h"
#include <chrono>
#include <imgui.h>
#include <stb_image.h>
#include "camera.h"

constexpr int MAX_FRAMES_IN_FLIGHT = 2;
static const std::filesystem::path WorkDir = std::filesystem::current_path().parent_path().parent_path().parent_path();
static const std::filesystem::path AssetsDir = WorkDir / "assets/";
static const std::filesystem::path ShadersDir = WorkDir / "shaders/";
static const std::filesystem::path gltfModelsDir = AssetsDir / "glTF-Sample-Assets/Models/";

struct Vertex {
    glm::vec3 pos;
    glm::vec2 texCoord;
};

struct UniformBufferObject
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct Buffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
};


Buffer createBuffer(VkDeviceSize size, VmaAllocator allocator, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags allocFlags = 0) {

    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.setSize(size)
        .setUsage(usage)
        .setSharingMode(vk::SharingMode::eExclusive);

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;
    allocInfo.flags = allocFlags;

    VkBuffer buffer{};
    VmaAllocation allocation{};

    VkResult result = vmaCreateBuffer(allocator, reinterpret_cast<VkBufferCreateInfo*>(&bufferInfo), &allocInfo, &buffer, &allocation, nullptr);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }
    return { buffer, allocation };
}

void createDepthImageAndView(VulkanContext& context, VkImage& depthImage, VmaAllocation& depthImageAllocation, vk::raii::ImageView& depthImageView, vk::Format depthFormat , Swapchain& swapchain) {
	// Create depth image

    VmaAllocationCreateInfo depthImageAllocInfo{};
    depthImageAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vk::ImageCreateInfo depthImageInfo{};
    depthImageInfo.setImageType(vk::ImageType::e2D)
        .setFormat(vk::Format::eD32Sfloat)
        .setExtent({ swapchain.getExtent().width, swapchain.getExtent().height, 1 })
        .setMipLevels(1)
        .setArrayLayers(1)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setInitialLayout(vk::ImageLayout::eUndefined);

    auto res = vmaCreateImage(context.allocator, reinterpret_cast<VkImageCreateInfo*>(&depthImageInfo), &depthImageAllocInfo, &depthImage, &depthImageAllocation, nullptr);
    if (res != VK_SUCCESS) throw std::runtime_error("failed to create depth image!");

    auto depthImageViewCreateInfo = vk::ImageViewCreateInfo()
        .setImage(depthImage)
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(vk::Format::eD32Sfloat)
        .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1));
    depthImageView = context.device.createImageView(depthImageViewCreateInfo);

}


static std::vector<char> readFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }
  std::vector<char> buffer(file.tellg());
  file.seekg(0, std::ios::beg);
  file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  file.close();
  return buffer;
}

 vk::raii::ShaderModule createShaderModule(const std::vector<char>& code,const vk::raii::Device& device) 
{
  vk::ShaderModuleCreateInfo smCreateInfo{};
  smCreateInfo.setCodeSize(code.size() * sizeof(char))
      .setPCode(reinterpret_cast<const uint32_t*>(code.data()));
  vk::raii::ShaderModule sm(device,smCreateInfo);
  return sm ; 
}


 int main() {

     Window window(1400, 1400, "Vulkan Renderer");
     Camera camera;
     camera.position = glm::vec3(2.0f, 2.0f, 2.0f);
     camera.fov = 45.0f;
     camera.nearPlane = 0.1f;
     camera.farPlane = 10.0f;
     camera.lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

     VulkanContext vulkanContext(window);

     const std::vector<Vertex> vertices = {
       {{ 0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}}, // bottom-right
       {{ 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f}}, // top-right
       {{-0.5f,  0.5f, 0.0f}, {0.0f, 1.0f}}, // top-left
       {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}} , // bottom-left

       {{ 0.5f, -0.5f, -0.4f },{1.0f, 0.0f}}, // bottom-right
       {{ 0.5f,  0.5f,-0.4f}, {1.0f, 1.0f}}, // top-right
       {{-0.5f,  0.5f, -0.4f}, {0.0f, 1.0f}}, // top-left
       {{-0.5f, -0.5f, -0.4f}, {0.0f, 0.0f}}  // bottom-left
     };

     // gltf asset 
     static constexpr auto supportedExtensions =
         fastgltf::Extensions::KHR_mesh_quantization |
         fastgltf::Extensions::KHR_texture_transform |
         fastgltf::Extensions::KHR_materials_variants;

     fastgltf::Parser parser(supportedExtensions);

     constexpr auto gltfOptions =
         fastgltf::Options::DontRequireValidAssetMember |
         fastgltf::Options::AllowDouble |
         fastgltf::Options::LoadExternalBuffers |
         fastgltf::Options::LoadExternalImages |
         fastgltf::Options::GenerateMeshIndices;

     auto gltfFilePath = gltfModelsDir / "DamagedHelmet" / "glTF" / "DamagedHelmet.gltf";

     auto gltfFile = fastgltf::MappedGltfFile::FromPath(gltfFilePath);
     if (!bool(gltfFile)) {
         std::cerr << "Failed to open glTF file: " << fastgltf::getErrorMessage(gltfFile.error()) << '\n';
         return false;
     }

     auto asset = parser.loadGltf(gltfFile.get(), gltfFilePath.parent_path(), gltfOptions);
     if (asset.error() != fastgltf::Error::None) {
         std::cerr << "Failed to load glTF: " << fastgltf::getErrorMessage(asset.error()) << '\n';
         return false;
     }

     Swapchain swapchain(vulkanContext, window);
     const std::vector<uint32_t> indices = { 0,1,2, 2,3,0 ,4,5,6, 6,7,4 };

     vk::VertexInputBindingDescription vertexBindingDescription{};
     vertexBindingDescription.binding = 0;
     vertexBindingDescription.stride = sizeof(Vertex);
     vertexBindingDescription.inputRate = vk::VertexInputRate::eVertex;

     std::array<vk::VertexInputAttributeDescription, 2> vertexAttributeDescriptions{}; // one for pos , one for color, one for texCoord
     vertexAttributeDescriptions[0].setBinding(0).setFormat(vk::Format::eR32G32B32Sfloat).setLocation(0).setOffset(offsetof(Vertex, pos));
     vertexAttributeDescriptions[1].setBinding(0).setFormat(vk::Format::eR32G32Sfloat).setLocation(1).setOffset(offsetof(Vertex, texCoord));

     VkDeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();
     VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();
     VkDeviceSize stagingBufferSize = vertexBufferSize + indexBufferSize;
     VkDeviceSize uniformBufferSize = sizeof(UniformBufferObject);
     std::vector<void*>                 uboUniformBuffersMapped;
     uboUniformBuffersMapped.reserve(MAX_FRAMES_IN_FLIGHT);

     std::vector<std::pair<vk::Buffer, VmaAllocation>> uboUniformBuffers;
     for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
         auto [uniformBuffer, uniformAlloc] = createBuffer(
             uniformBufferSize, vulkanContext.allocator,
             vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_AUTO,
             VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
         uboUniformBuffers.emplace_back(uniformBuffer, uniformAlloc);

         VmaAllocationInfo allocInfo{};
         vmaGetAllocationInfo(vulkanContext.allocator, uniformAlloc, &allocInfo);
         uboUniformBuffersMapped.push_back(allocInfo.pMappedData); // it is a void* pointing to buffers memeory 

     }

     // image for depth texture (filled directly in GPU
	 VmaAllocation depthImageAlloc{};
	 VkImage depthImage{};
     vk::raii::ImageView depthImageView{nullptr};


     // load texture ( in CPU)
	 int texWidth, texHeight, texChannels;
	 stbi_uc* pixels = stbi_load((AssetsDir / "pattern.jpg").string().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	 vk::DeviceSize imageSize = texWidth * texHeight * 4;
     if (!pixels)
     {
		 throw std::runtime_error("failed to load texture image!");
     }
	 // create staging buffer for texture data(host visible)
	 auto [texStagingBuffer, texStagingAlloc] = createBuffer(imageSize, vulkanContext.allocator, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
	 // copy texture data to staging buffer
	 VkResult vr = vmaCopyMemoryToAllocation(vulkanContext.allocator, pixels, texStagingAlloc, 0, imageSize);
	 if (vr != VK_SUCCESS) throw std::runtime_error("failed to copy texture data to staging buffer!");

     stbi_image_free(pixels);

     // image ( device local buffer) 
	 VkImage textureImage{};
	 vk::ImageCreateInfo imageInfo{};
	 VmaAllocation imageAlloc{};
     VmaAllocationCreateInfo imageAllocInfo{};
     imageAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	 imageInfo.setImageType(vk::ImageType::e2D)
		 .setFormat(vk::Format::eR8G8B8A8Srgb)
		 .setExtent({ static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1 })
		 .setMipLevels(1)
		 .setArrayLayers(1)
		 .setSamples(vk::SampleCountFlagBits::e1)
		 .setTiling(vk::ImageTiling::eOptimal)
		 .setUsage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
		 .setSharingMode(vk::SharingMode::eExclusive)
		 .setInitialLayout(vk::ImageLayout::eUndefined);
	
	  vr = vmaCreateImage(vulkanContext.allocator, reinterpret_cast<VkImageCreateInfo*>(&imageInfo), &imageAllocInfo,&textureImage, &imageAlloc, nullptr);
     if (vr != VK_SUCCESS)
         throw std::runtime_error("Failed to create image!");



  auto [stagingBuffer, stagingAlloc] = createBuffer(stagingBufferSize, vulkanContext.allocator, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
  auto [vertexBuffer, vertexAlloc] = createBuffer(vertexBufferSize, vulkanContext.allocator, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
  auto [indexBuffer, indexAlloc] = createBuffer(indexBufferSize, vulkanContext.allocator, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

  

  VkResult vrrr = vmaCopyMemoryToAllocation(vulkanContext.allocator, vertices.data(), stagingAlloc, 0, vertexBufferSize);
  if (vrrr   != VK_SUCCESS) throw std::runtime_error("failed to copy vertex data to staging buffer!");

  vr = vmaCopyMemoryToAllocation(vulkanContext.allocator, indices.data(), stagingAlloc, vertexBufferSize, indexBufferSize);
  if (vr != VK_SUCCESS) throw std::runtime_error("failed to copy index data to staging buffer!");


  // begin single time command buf
  vk::CommandBufferAllocateInfo copyCmdAllocInfo{};
  copyCmdAllocInfo.setCommandPool(*vulkanContext.commandPool)
      .setLevel(vk::CommandBufferLevel::ePrimary)
      .setCommandBufferCount(1);

  vk::raii::CommandBuffers copyCmdBufs(vulkanContext.device, copyCmdAllocInfo);
  vk::raii::CommandBuffer copyCmdBuf = std::move(copyCmdBufs.front());

  copyCmdBuf.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

  // copy vertex and index data from staging buffer to device local buffers

  copyCmdBuf.copyBuffer(stagingBuffer, vertexBuffer, vk::BufferCopy(0,0,vertexBufferSize));
  copyCmdBuf.copyBuffer(stagingBuffer, indexBuffer, vk::BufferCopy(vertexBufferSize, 0, indexBufferSize));

  vk::ImageMemoryBarrier barrier{};
  barrier.setOldLayout(vk::ImageLayout::eUndefined)
	  .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
	  .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
	  .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
	  .setImage(textureImage)
	  .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1))
	  .setSrcAccessMask({})
	  .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
  copyCmdBuf.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

  copyCmdBuf.copyBufferToImage(texStagingBuffer,textureImage, vk::ImageLayout::eTransferDstOptimal,
      vk::BufferImageCopy(0, 0, 0, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
          vk::Offset3D(0, 0, 0), vk::Extent3D(texWidth, texHeight, 1)));

  // transition image to shader read optimal
  barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal)
	  .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
	  .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
	  .setDstAccessMask(vk::AccessFlagBits::eShaderRead);

  copyCmdBuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

  // end single time command buf
  copyCmdBuf.end();

  vk::SubmitInfo copySubmitInfo{};
  copySubmitInfo.setCommandBuffers(*copyCmdBuf);
  vulkanContext.graphicsQueue.submit(copySubmitInfo);
  vulkanContext.graphicsQueue.waitIdle(); 



  // create image view for the texture image
  vk::ImageViewCreateInfo imageViewInfo{};
  imageViewInfo.setImage(textureImage)
	  .setViewType(vk::ImageViewType::e2D)
	  .setFormat(vk::Format::eR8G8B8A8Srgb)
	  .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

  vk::raii::ImageView textureImageView(vulkanContext.device, imageViewInfo);

  // sampler 
  vk::SamplerCreateInfo samplerInfo{};
  samplerInfo.setMagFilter(vk::Filter::eLinear)
      .setMinFilter(vk::Filter::eLinear)
      .setAddressModeU(vk::SamplerAddressMode::eRepeat)
      .setAddressModeV(vk::SamplerAddressMode::eRepeat)
      .setAddressModeW(vk::SamplerAddressMode::eRepeat)
      .setAnisotropyEnable(VK_TRUE)
      .setMaxAnisotropy(vulkanContext.physicalDevice.getProperties().limits.maxSamplerAnisotropy)
      .setBorderColor(vk::BorderColor::eIntOpaqueBlack)
      .setUnnormalizedCoordinates(VK_FALSE)
      .setCompareEnable(VK_FALSE)
      .setCompareOp(vk::CompareOp::eAlways)
      .setMipmapMode(vk::SamplerMipmapMode::eLinear)
      .setMipLodBias(0.0f)
      .setMinLod(0.0f)
      .setMaxLod(0.0f);

  auto sampler = vk::raii::Sampler(vulkanContext.device, samplerInfo);


  // pipeline
  // descriptor pool

  std::array<vk::DescriptorPoolSize, 2> poolSize{};
  poolSize[0].setType(vk::DescriptorType::eUniformBuffer)
      .setDescriptorCount(MAX_FRAMES_IN_FLIGHT);
  poolSize[1].setType(vk::DescriptorType::eCombinedImageSampler)
      .setDescriptorCount(MAX_FRAMES_IN_FLIGHT);

  vk::DescriptorPoolCreateInfo poolInfo{};
  poolInfo.setPoolSizeCount(2)
      .setPPoolSizes(poolSize.data())
      .setMaxSets(MAX_FRAMES_IN_FLIGHT)
      .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);

  vk::raii::DescriptorPool descriptorPool(vulkanContext.device, poolInfo);

  // descriptor pipeline layout

  vk::DescriptorSetLayoutBinding uboLayoutBinding{};
  uboLayoutBinding.setBinding(0)
      .setDescriptorType(vk::DescriptorType::eUniformBuffer)
      .setDescriptorCount(1)
      .setStageFlags(vk::ShaderStageFlagBits::eVertex);
  vk::DescriptorSetLayoutBinding samplerLayoutBinding{};
  samplerLayoutBinding.setBinding(1)
	  .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
	  .setDescriptorCount(1)
	  .setStageFlags(vk::ShaderStageFlagBits::eFragment);
  std::array<vk::DescriptorSetLayoutBinding, 2> bindings{uboLayoutBinding, samplerLayoutBinding};

  // descriptor set

  vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
  descriptorSetLayoutCreateInfo.setBindingCount(bindings.size()).setPBindings(bindings.data());
  vk::raii::DescriptorSetLayout descriptorSetLayout = vulkanContext.device.createDescriptorSetLayout(descriptorSetLayoutCreateInfo);
  vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
  pipelineLayoutCreateInfo.setSetLayoutCount(1).setPushConstantRangeCount(0).setPSetLayouts(&*descriptorSetLayout);
  auto pipelineLayout = vulkanContext.device.createPipelineLayout(pipelineLayoutCreateInfo);
  std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);

  vk::DescriptorSetAllocateInfo dsAllocInfo{};
  dsAllocInfo.setDescriptorPool(*descriptorPool)
      .setDescriptorSetCount(MAX_FRAMES_IN_FLIGHT)
      .setPSetLayouts(layouts.data());

  vk::raii::DescriptorSets descriptorSets(vulkanContext.device, dsAllocInfo);

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      vk::DescriptorBufferInfo bufferInfo{};
      bufferInfo.setBuffer(uboUniformBuffers[i].first)
          .setOffset(0)
          .setRange(sizeof(UniformBufferObject));

	  vk::DescriptorImageInfo imageInfo{};
	  imageInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
		  .setImageView(*textureImageView)
		  .setSampler(*sampler);

      vk::WriteDescriptorSet writeUniform{};
      writeUniform.setDstSet(*descriptorSets[i])
          .setDstBinding(0)
          .setDstArrayElement(0)
          .setDescriptorType(vk::DescriptorType::eUniformBuffer)
          .setDescriptorCount(1)
          .setPBufferInfo(&bufferInfo);
      vk::WriteDescriptorSet writeSampler{};
      writeSampler.setDstSet(*descriptorSets[i])
          .setDstBinding(1)
          .setDstArrayElement(0)
          .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
          .setDescriptorCount(1)
          .setPImageInfo(&imageInfo);
	  std::array<vk::WriteDescriptorSet, 2> descriptorWrites = { writeUniform, writeSampler };
      vulkanContext.device.updateDescriptorSets(descriptorWrites, {});
  }

  auto shaderModule = createShaderModule(readFile((ShadersDir / "test.spv").string()), vulkanContext.device);

  vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.setPName("vertMain")
      .setStage(vk::ShaderStageFlagBits::eVertex)
      .setModule(shaderModule);

  vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.setPName("fragMain")
      .setStage(vk::ShaderStageFlagBits::eFragment)
      .setModule(shaderModule);

  vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
  vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
  vertexInputInfo.setVertexBindingDescriptionCount(1)
	  .setPVertexBindingDescriptions(&vertexBindingDescription)
	  .setVertexAttributeDescriptionCount(static_cast<uint32_t>(vertexAttributeDescriptions.size()))
	  .setPVertexAttributeDescriptions(vertexAttributeDescriptions.data());

  vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList);

  std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

  // flipped to match open gl , glm , fast gltf convention
  vk::PipelineViewportStateCreateInfo viewportState{};
  viewportState.setViewportCount(1).setScissorCount(1);

  vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
  pipelineDynamicStateCreateInfo.setDynamicStateCount(dynamicStates.size())
      .setPDynamicStates(dynamicStates.data());

  vk::PipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
  rasterizationStateCreateInfo.setPolygonMode(vk::PolygonMode::eFill)
      .setCullMode(vk::CullModeFlagBits::eBack)
      .setFrontFace(vk::FrontFace::eCounterClockwise)
      .setLineWidth(1.0f)
      .setDepthBiasEnable(VK_FALSE)
      .setRasterizerDiscardEnable(VK_FALSE)
      .setDepthClampEnable(VK_FALSE);

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
        .setBlendEnable(VK_FALSE);

    vk::PipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
    colorBlendStateCreateInfo.setLogicOpEnable(VK_FALSE).setAttachmentCount(1).setPAttachments(&colorBlendAttachment).setLogicOp(vk::LogicOp::eCopy);

    vk::PipelineMultisampleStateCreateInfo multisampleStateCreateInfo{};
    multisampleStateCreateInfo
        .setRasterizationSamples(vk::SampleCountFlagBits::e1)
        .setSampleShadingEnable(VK_FALSE);

	vk::PipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{};
	depthStencilStateCreateInfo.setDepthTestEnable(VK_TRUE)
		.setDepthWriteEnable(VK_TRUE)
		.setDepthCompareOp(vk::CompareOp::eLess)
		.setDepthBoundsTestEnable(VK_FALSE)
		.setStencilTestEnable(VK_FALSE);

	auto colorFormat = swapchain.getFormat();
    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo
        .setColorAttachmentFormats({colorFormat})
		.setDepthAttachmentFormat(vk::Format::eD32Sfloat);

    vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
    graphicsPipelineCreateInfo.setStageCount(2)
        .setPStages(shaderStages)
        .setPVertexInputState(&vertexInputInfo)
        .setPInputAssemblyState(&inputAssembly)
        .setPRasterizationState(&rasterizationStateCreateInfo)
        .setPMultisampleState(&multisampleStateCreateInfo)
        .setPDynamicState(&pipelineDynamicStateCreateInfo)
        .setPDepthStencilState(&depthStencilStateCreateInfo)
        .setLayout(pipelineLayout)
        .setRenderPass(nullptr)
        .setPColorBlendState(&colorBlendStateCreateInfo)
        .setPViewportState(&viewportState);
   
   const vk::StructureChain<vk::GraphicsPipelineCreateInfo,vk::PipelineRenderingCreateInfo> chain{graphicsPipelineCreateInfo,pipelineRenderingCreateInfo};
   vk::raii::Pipeline pipeline{vulkanContext.device, nullptr,chain.get<vk::GraphicsPipelineCreateInfo>()};

   std::vector<vk::raii::Semaphore> acquireSemaphores;
   std::vector<vk::raii::Semaphore> presentSemaphores;
   for (size_t i = 0; i < swapchain.getImagesCount(); i++) {
       acquireSemaphores.emplace_back(vulkanContext.device, vk::SemaphoreCreateInfo());
       presentSemaphores.emplace_back(vulkanContext.device, vk::SemaphoreCreateInfo());
   }

   vk::FenceCreateInfo fenceInfo{};
   fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);
   std::vector<vk::raii::Fence> drawFences;
   for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
 
       drawFences.emplace_back(vulkanContext.device, fenceInfo);
   }

   vk::CommandBufferAllocateInfo commandBufferAllocateInfo{};
   commandBufferAllocateInfo.setCommandPool(*vulkanContext.commandPool)
       .setCommandBufferCount(MAX_FRAMES_IN_FLIGHT)
       .setLevel(vk::CommandBufferLevel::ePrimary);

   auto commandBuffers = std::move(vk::raii::CommandBuffers(vulkanContext.device, commandBufferAllocateInfo));
   auto clearColorValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
   auto depthClear = vk::ClearDepthStencilValue(1.0f, 0);
   createDepthImageAndView(vulkanContext, depthImage, depthImageAlloc, depthImageView, vk::Format::eD32Sfloat, swapchain);

   vk::RenderingAttachmentInfo renderingAttachmentInfo{};
   vk::RenderingInfo renderingInfo{};

   size_t currentFrame = 0;

   UniformBufferObject cameraUbo{};

   while (!window.shouldClose()) {
       glfwPollEvents();
    
       auto fenceResult = vulkanContext.device.waitForFences(*drawFences[currentFrame], VK_TRUE, UINT64_MAX);
        if (fenceResult != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to wait for fence");
        }
        vk::Result result{};
        uint32_t imageIndex{};

        try {
             std::tie(result, imageIndex) = vulkanContext.device.acquireNextImage2KHR(
            vk::AcquireNextImageInfoKHR(swapchain.get(), UINT64_MAX, *acquireSemaphores[currentFrame], nullptr, 1));
             
        }
        catch (const vk::OutOfDateKHRError)
        {
            swapchain.recreate(window);
            vmaDestroyImage(vulkanContext.allocator, depthImage, depthImageAlloc);
			createDepthImageAndView(vulkanContext, depthImage, depthImageAlloc, depthImageView, vk::Format::eD32Sfloat, swapchain);
            continue;
        }
        
       vulkanContext.device.resetFences(*drawFences[currentFrame]);

       vk::Rect2D scissor(vk::Offset2D{ 0, 0 }, swapchain.getExtent());
       vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(swapchain.getExtent().width),
           static_cast<float>(swapchain.getExtent().height), 0.0f, 1.0f);


       auto depthAttachmentInfo = vk::RenderingAttachmentInfo()
           .setImageView(*depthImageView)
           .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
           .setLoadOp(vk::AttachmentLoadOp::eClear)
           .setStoreOp(vk::AttachmentStoreOp::eDontCare)
           .setClearValue(depthClear);

       renderingAttachmentInfo.setImageView(swapchain.getImageViews()[imageIndex])
           .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
           .setLoadOp(vk::AttachmentLoadOp::eClear)
           .setStoreOp(vk::AttachmentStoreOp::eStore)
           .setClearValue(clearColorValue);

       renderingInfo
           .setRenderArea({ {0,0}, swapchain.getExtent() })
           .setLayerCount(1)
           .setColorAttachments(renderingAttachmentInfo)
		   .setPDepthAttachment(&depthAttachmentInfo);

       static auto startTime = std::chrono::high_resolution_clock::now();
       auto currentTime = std::chrono::high_resolution_clock::now();
       float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	   auto aspectRatio = static_cast<float>(swapchain.getExtent().width) / static_cast<float>(swapchain.getExtent().height);

       cameraUbo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
       cameraUbo.view = camera.getViewMatrix();
       cameraUbo.proj = camera.getProjectionMatrix(aspectRatio);

       std::memcpy(uboUniformBuffersMapped[currentFrame], &cameraUbo, sizeof(cameraUbo));

       commandBuffers[currentFrame].reset();
       commandBuffers[currentFrame].begin(vk::CommandBufferBeginInfo{});

       vk::ImageMemoryBarrier2 toColorAttachment{};
       toColorAttachment.setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
           .setSrcAccessMask({})
           .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
           .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
           .setOldLayout(vk::ImageLayout::eUndefined)
           .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
           .setImage(swapchain.getImages()[imageIndex])
           .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

	   vk::ImageMemoryBarrier2 toDepthAttachment{};
	   toDepthAttachment.setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
		   .setSrcAccessMask({})
		   .setDstStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests|vk::PipelineStageFlagBits2::eLateFragmentTests)
		   .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
		   .setOldLayout(vk::ImageLayout::eUndefined)
		   .setNewLayout(vk::ImageLayout::eDepthAttachmentOptimal)
		   .setImage(depthImage)
		   .setSubresourceRange({ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });

       vk::DependencyInfo depInfoToColor{};
       depInfoToColor.setImageMemoryBarrierCount(1).setPImageMemoryBarriers(&toColorAttachment);
	   vk::DependencyInfo depInfoToDepth{};
	   depInfoToDepth.setImageMemoryBarrierCount(1).setPImageMemoryBarriers(&toDepthAttachment);
       commandBuffers[currentFrame].pipelineBarrier2(depInfoToDepth);
       commandBuffers[currentFrame].pipelineBarrier2(depInfoToColor);
       commandBuffers[currentFrame].beginRendering(renderingInfo);
       commandBuffers[currentFrame].setViewport(0, viewport);
       commandBuffers[currentFrame].setScissor(0, scissor);
       commandBuffers[currentFrame].bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
       commandBuffers[currentFrame].bindDescriptorSets( vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0,*descriptorSets[currentFrame], {});
       commandBuffers[currentFrame].bindVertexBuffers(0, { vertexBuffer }, { 0 });
       commandBuffers[currentFrame].bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint32);
       commandBuffers[currentFrame].drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
       commandBuffers[currentFrame].endRendering();

       vk::ImageMemoryBarrier2 toPresent{};
       toPresent.setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
           .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
           .setDstStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe)
           .setDstAccessMask({})
           .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
           .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
           .setImage(swapchain.getImages()[imageIndex])
           .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

       vk::DependencyInfo depInfoToPresent{};
       depInfoToPresent.setImageMemoryBarrierCount(1).setPImageMemoryBarriers(&toPresent);
       commandBuffers[currentFrame].pipelineBarrier2(depInfoToPresent);
       commandBuffers[currentFrame].end();

       // only now: submit + present
       vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
       const vk::SubmitInfo submitInfo(*acquireSemaphores[currentFrame], waitDestinationStageMask, *commandBuffers[currentFrame], *presentSemaphores[imageIndex]);
       vulkanContext.graphicsQueue.submit(submitInfo, *drawFences[currentFrame]);

       vk::PresentInfoKHR presentInfo{};
       presentInfo.pSwapchains = &swapchain.get();
       presentInfo.swapchainCount = 1;
       presentInfo.pImageIndices = &imageIndex;
       presentInfo.waitSemaphoreCount = 1;
	   presentInfo.pWaitSemaphores = &(*presentSemaphores[imageIndex]);

       try {
           auto presentResult = vulkanContext.graphicsQueue.presentKHR(presentInfo);

       }
       catch (const vk::OutOfDateKHRError) {
		   swapchain.recreate(window);
           vmaDestroyImage(vulkanContext.allocator, depthImage, depthImageAlloc);
           createDepthImageAndView(vulkanContext, depthImage, depthImageAlloc, depthImageView, vk::Format::eD32Sfloat, swapchain);
           continue;
        
       }
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

    }

	vulkanContext.device.waitIdle();
    vmaDestroyBuffer(vulkanContext.allocator, stagingBuffer, stagingAlloc);
    vmaDestroyBuffer(vulkanContext.allocator, vertexBuffer, vertexAlloc);
    vmaDestroyBuffer(vulkanContext.allocator, indexBuffer, indexAlloc);
    vmaDestroyImage(vulkanContext.allocator, depthImage, depthImageAlloc);
	vmaDestroyBuffer(vulkanContext.allocator, texStagingBuffer, texStagingAlloc);
	vmaDestroyImage(vulkanContext.allocator, textureImage, imageAlloc);
	for (const auto& [buffer, allocation] : uboUniformBuffers) {
		vmaDestroyBuffer(vulkanContext.allocator, buffer, allocation);
	}

    return 0 ;
}