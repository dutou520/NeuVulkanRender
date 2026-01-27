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
   * @brief 创建 GBuffer 资源
   * @param device 逻辑设备
   * @param physicalDevice 物理设备
   * @param width 宽度
   * @param height 高度
   */
  void Create(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width,
              uint32_t height);

  /**
   * @brief 销毁 GBuffer 资源
   * @param device 逻辑设备
   */
  void Destroy(VkDevice device);

  /**
   * @brief 检查 GBuffer 是否有效
   */
  bool IsValid() const { return m_IsValid; }

  /**
   * @brief 获取宽度
   */
  uint32_t GetWidth() const { return m_Width; }

  /**
   * @brief 获取高度
   */
  uint32_t GetHeight() const { return m_Height; }

  // GBuffer 附件访问
  GBufferAttachment &GetAlbedoFlags() { return m_AlbedoMaterialFlags; }
  GBufferAttachment &GetSpecularOcclusion() { return m_SpecularOcclusion; }
  GBufferAttachment &GetNormalSmoothness() { return m_NormalSmoothness; }
  GBufferAttachment &GetShadingEmissive() { return m_ShadingIdEmissive; }
  GBufferAttachment &GetDepth() { return m_Depth; }

  const GBufferAttachment &GetAlbedoFlags() const {
    return m_AlbedoMaterialFlags;
  }
  const GBufferAttachment &GetSpecularOcclusion() const {
    return m_SpecularOcclusion;
  }
  const GBufferAttachment &GetNormalSmoothness() const {
    return m_NormalSmoothness;
  }
  const GBufferAttachment &GetShadingEmissive() const {
    return m_ShadingIdEmissive;
  }
  const GBufferAttachment &GetDepth() const { return m_Depth; }

  /**
   * @brief 获取所有颜色附件的 ImageViews (用于 Framebuffer)
   */
  std::vector<VkImageView> GetColorAttachmentViews() const;

  /**
   * @brief 获取深度附件的 ImageView
   */
  VkImageView GetDepthView() const { return m_Depth.view; }

  // RenderPass 和 Framebuffer
  VkRenderPass renderPass = VK_NULL_HANDLE;
  VkFramebuffer framebuffer = VK_NULL_HANDLE;

private:
  void CreateAttachment(VkDevice device, VkPhysicalDevice physicalDevice,
                        VkFormat format, VkImageUsageFlags usage,
                        VkImageAspectFlags aspectFlags,
                        GBufferAttachment &attachment);

  void CreateSampler(VkDevice device, GBufferAttachment &attachment);

  uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                          VkMemoryPropertyFlags properties);

  GBufferAttachment m_AlbedoMaterialFlags; // GBuffer1: Albedo + MaterialFlags
  GBufferAttachment m_SpecularOcclusion;   // GBuffer2: Specular + Occlusion
  GBufferAttachment m_NormalSmoothness;    // GBuffer3: Normal + Smoothness
  GBufferAttachment m_ShadingIdEmissive;   // GBuffer4: ShadingID + Emissive
  GBufferAttachment m_Depth;               // Depth attachment

  uint32_t m_Width = 0;
  uint32_t m_Height = 0;
  bool m_IsValid = false;
};

} // namespace neurender
