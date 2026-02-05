#include "GBuffer.h"
#include "neuLog.h"
#include <stdexcept>

namespace neurender {

void GBuffer::Create(VkDevice device, VkPhysicalDevice physicalDevice,
                     uint32_t width, uint32_t height, int framesInFlight) {
  m_Width = width;
  m_Height = height;
  m_FramesInFlight = framesInFlight;

  // Resize vectors to hold resources for each frame
  m_AlbedoMaterialFlags.resize(framesInFlight);
  m_SpecularOcclusion.resize(framesInFlight);
  m_NormalSmoothness.resize(framesInFlight);
  m_ShadingIdEmissive.resize(framesInFlight);
  m_Depth.resize(framesInFlight);

  // Resize Framebuffers vector
  m_Framebuffers.resize(framesInFlight);

  for (int i = 0; i < framesInFlight; i++) {
    // GBuffer1: Albedo + MaterialFlags (sRGB)
    CreateAttachment(device, physicalDevice, VK_FORMAT_R8G8B8A8_SRGB,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT, m_AlbedoMaterialFlags[i]);
    CreateSampler(device, m_AlbedoMaterialFlags[i]);

    // GBuffer2: Specular + Occlusion (UNORM)
    CreateAttachment(device, physicalDevice, VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT, m_SpecularOcclusion[i]);
    CreateSampler(device, m_SpecularOcclusion[i]);

    // GBuffer3: Normal + Smoothness (UNORM)
    CreateAttachment(device, physicalDevice, VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT, m_NormalSmoothness[i]);
    CreateSampler(device, m_NormalSmoothness[i]);

    // GBuffer4: ShadingID + Emissive (UNORM)
    CreateAttachment(device, physicalDevice, VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT, m_ShadingIdEmissive[i]);
    CreateSampler(device, m_ShadingIdEmissive[i]);

    // Depth: D32_SFLOAT
    CreateAttachment(device, physicalDevice, VK_FORMAT_D32_SFLOAT,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_IMAGE_ASPECT_DEPTH_BIT, m_Depth[i]);
    CreateSampler(device, m_Depth[i]);
  }

  m_IsValid = true;
  LOG_I("GBuffer created: {}x{} with {} frames", width, height, framesInFlight);
}

void GBuffer::Destroy(VkDevice device) {
  if (!m_IsValid)
    return;

  auto destroyAttachment = [device](GBufferAttachment &attachment) {
    if (attachment.sampler != VK_NULL_HANDLE) {
      vkDestroySampler(device, attachment.sampler, nullptr);
      attachment.sampler = VK_NULL_HANDLE;
    }
    if (attachment.view != VK_NULL_HANDLE) {
      vkDestroyImageView(device, attachment.view, nullptr);
      attachment.view = VK_NULL_HANDLE;
    }
    if (attachment.image != VK_NULL_HANDLE) {
      vkDestroyImage(device, attachment.image, nullptr);
      attachment.image = VK_NULL_HANDLE;
    }
    if (attachment.memory != VK_NULL_HANDLE) {
      vkFreeMemory(device, attachment.memory, nullptr);
      attachment.memory = VK_NULL_HANDLE;
    }
  };

  for (int i = 0; i < m_FramesInFlight; i++) {
    destroyAttachment(m_AlbedoMaterialFlags[i]);
    destroyAttachment(m_SpecularOcclusion[i]);
    destroyAttachment(m_NormalSmoothness[i]);
    destroyAttachment(m_ShadingIdEmissive[i]);
    destroyAttachment(m_Depth[i]);

    if (m_Framebuffers[i] != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device, m_Framebuffers[i], nullptr);
      m_Framebuffers[i] = VK_NULL_HANDLE;
    }
  }

  // Clear vectors
  m_AlbedoMaterialFlags.clear();
  m_SpecularOcclusion.clear();
  m_NormalSmoothness.clear();
  m_ShadingIdEmissive.clear();
  m_Depth.clear();
  m_Framebuffers.clear();

  if (renderPass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device, renderPass, nullptr);
    renderPass = VK_NULL_HANDLE;
  }

  m_IsValid = false;
  LOG_I("GBuffer destroyed");
}

std::vector<VkImageView>
GBuffer::GetColorAttachmentViews(int frameIndex) const {
  return {m_AlbedoMaterialFlags[frameIndex].view,
          m_SpecularOcclusion[frameIndex].view,
          m_NormalSmoothness[frameIndex].view,
          m_ShadingIdEmissive[frameIndex].view};
}

void GBuffer::CreateAttachment(VkDevice device, VkPhysicalDevice physicalDevice,
                               VkFormat format, VkImageUsageFlags usage,
                               VkImageAspectFlags aspectFlags,
                               GBufferAttachment &attachment) {
  attachment.format = format;

  // Create image
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = m_Width;
  imageInfo.extent.height = m_Height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = format;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateImage(device, &imageInfo, nullptr, &attachment.image) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create GBuffer image!");
  }

  // Allocate memory
  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(device, attachment.image, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex =
      FindMemoryType(physicalDevice, memRequirements.memoryTypeBits,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  if (vkAllocateMemory(device, &allocInfo, nullptr, &attachment.memory) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate GBuffer image memory!");
  }

  vkBindImageMemory(device, attachment.image, attachment.memory, 0);

  // Create image view
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = attachment.image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = format;
  viewInfo.subresourceRange.aspectMask = aspectFlags;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(device, &viewInfo, nullptr, &attachment.view) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create GBuffer image view!");
  }
}

void GBuffer::CreateSampler(VkDevice device, GBufferAttachment &attachment) {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_NEAREST;
  samplerInfo.minFilter = VK_FILTER_NEAREST;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.maxAnisotropy = 1.0f;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = 0.0f;

  if (vkCreateSampler(device, &samplerInfo, nullptr, &attachment.sampler) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create GBuffer sampler!");
  }
}

uint32_t GBuffer::FindMemoryType(VkPhysicalDevice physicalDevice,
                                 uint32_t typeFilter,
                                 VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("Failed to find suitable memory type!");
}

} // namespace neurender
