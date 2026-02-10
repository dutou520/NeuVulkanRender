#pragma once

#include <vector>
#include <vulkan/vulkan.h>

namespace neurender {

/**
 * @brief GBuffer 单个附件
 */
struct GBufferAttachment {
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkFormat format = VK_FORMAT_UNDEFINED;
  VkSampler sampler = VK_NULL_HANDLE;
};

/**
 * @brief GBuffer 系统
 *
 * 管理延迟渲染所需的 G-Buffer 资源
 *
 * GBuffer 布局:
 * - GBuffer1: RGB=Albedo(sRGB), A=MaterialFlags  (VK_FORMAT_R8G8B8A8_SRGB)
 * - GBuffer2: RGB=Specular, A=Occlusion          (VK_FORMAT_R8G8B8A8_UNORM)
 * - GBuffer3: RGB=Normal, A=Smoothness           (VK_FORMAT_R8G8B8A8_UNORM)
 * - GBuffer4: R=ShadingID, GB=Emissive Brightness, A=Emissive Hue
 * (VK_FORMAT_R8G8B8A8_UNORM)
 * - Depth:    D32_SFLOAT
 */
class GBuffer {
public:
  /**
   * @brief Create GBuffer resources
   * @param device Logical device
   * @param physicalDevice Physical device
   * @param width Width
   * @param height Height
   * @param framesInFlight Number of frames in flight (for double buffering)
   */
  void Create(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width,
              uint32_t height, int framesInFlight);

  /**
   * @brief Destroy GBuffer resources
   * @param device Logical device
   */
  void Destroy(VkDevice device);

  /**
   * @brief Check if GBuffer is valid
   */
  bool IsValid() const { return m_IsValid; }

  /**
   * @brief Get Width
   */
  uint32_t GetWidth() const { return m_Width; }

  /**
   * @brief Get Height
   */
  uint32_t GetHeight() const { return m_Height; }

  // GBuffer Attachment Access (per frame)
  GBufferAttachment &GetAlbedoFlags(int frameIndex) {
    return m_AlbedoMaterialFlags[frameIndex];
  }
  GBufferAttachment &GetSpecularOcclusion(int frameIndex) {
    return m_SpecularOcclusion[frameIndex];
  }
  GBufferAttachment &GetNormalSmoothness(int frameIndex) {
    return m_NormalSmoothness[frameIndex];
  }
  GBufferAttachment &GetShadingEmissive(int frameIndex) {
    return m_ShadingIdEmissive[frameIndex];
  }
  GBufferAttachment &GetDepth(int frameIndex) { return m_Depth[frameIndex]; }

  const GBufferAttachment &GetAlbedoFlags(int frameIndex) const {
    return m_AlbedoMaterialFlags[frameIndex];
  }
  const GBufferAttachment &GetSpecularOcclusion(int frameIndex) const {
    return m_SpecularOcclusion[frameIndex];
  }
  const GBufferAttachment &GetNormalSmoothness(int frameIndex) const {
    return m_NormalSmoothness[frameIndex];
  }
  const GBufferAttachment &GetShadingEmissive(int frameIndex) const {
    return m_ShadingIdEmissive[frameIndex];
  }
  const GBufferAttachment &GetDepth(int frameIndex) const {
    return m_Depth[frameIndex];
  }

  /**
   * @brief Get all color attachment ImageViews for a specific frame (for
   * Framebuffer)
   */
  std::vector<VkImageView> GetColorAttachmentViews(int frameIndex) const;

  /**
   * @brief Get depth attachment ImageView for a specific frame
   */
  VkImageView GetDepthView(int frameIndex) const {
    return m_Depth[frameIndex].view;
  }

  /**
   * @brief Get Framebuffer for a specific frame
   */
  VkFramebuffer GetFramebuffer(int frameIndex) const {
    return m_Framebuffers[frameIndex];
  }

  // Clear images and framebuffers but keep renderPass
  void ClearResources(VkDevice device);

  // Set Framebuffer (called by RenderCore)
  void SetFramebuffer(int frameIndex, VkFramebuffer fb) {
    m_Framebuffers[frameIndex] = fb;
  }
  void ResizeFramebuffers(int size) { m_Framebuffers.resize(size); }

  // RenderPass is shared across frames (compatible)
  VkRenderPass renderPass = VK_NULL_HANDLE;

private:
  void CreateAttachment(VkDevice device, VkPhysicalDevice physicalDevice,
                        VkFormat format, VkImageUsageFlags usage,
                        VkImageAspectFlags aspectFlags,
                        GBufferAttachment &attachment);

  void CreateSampler(VkDevice device, GBufferAttachment &attachment);

  uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                          VkMemoryPropertyFlags properties);

  // Arrays of attachments for double buffering
  std::vector<GBufferAttachment> m_AlbedoMaterialFlags;
  std::vector<GBufferAttachment> m_SpecularOcclusion;
  std::vector<GBufferAttachment> m_NormalSmoothness;
  std::vector<GBufferAttachment> m_ShadingIdEmissive;
  std::vector<GBufferAttachment> m_Depth;

  std::vector<VkFramebuffer> m_Framebuffers;

  uint32_t m_Width = 0;
  uint32_t m_Height = 0;
  int m_FramesInFlight = 0;
  bool m_IsValid = false;
};

} // namespace neurender
