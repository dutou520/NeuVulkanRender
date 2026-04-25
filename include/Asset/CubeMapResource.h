#pragma once

#include "Core/UUID.h"
#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace neurender {

/**
 * @brief CubeMap 纹理资源类
 * 支持加载 6 张图片作为 Vulkan CubeMap 资源
 */
struct CubeMapResource {
  UUID uuid;
  std::string name;
  std::vector<std::string> filePaths; // 6 个面的路径

  // Vulkan 资源句柄
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView imageView = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

  // 预过滤环境贴图 (IBL Specular, 6 mip levels)
  VkImage prefilteredImage = VK_NULL_HANDLE;
  VkDeviceMemory prefilteredMemory = VK_NULL_HANDLE;
  VkImageView prefilteredImageView = VK_NULL_HANDLE;
  VkSampler prefilteredSampler = VK_NULL_HANDLE;
  static constexpr uint32_t PREFILTER_MIP_LEVELS = 6;

  bool m_hasIBLCache = false;
  std::vector<uint8_t> m_prefilterCacheData;

  bool LoadIBLCache(const std::string &path, uint64_t sourceTime);
  bool SaveIBLCache(const std::string &path, uint64_t sourceTime,
                    const uint8_t *prefilterData, size_t prefilterSize);

  // 球谐系数 (9个，xyz存RGB)
  glm::vec4 sh[9] = {};

  // 纹理属性
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t channels = 4;
  uint32_t mipLevels = 1;

  // 加载状态
  bool isLoaded = false;

  /**
   * @brief 从 6 个文件加载 CubeMap
   * @param paths 包含 6 个面的路径，顺序：Right(PosX), Left(NegX), Top(PosY),
   * Bottom(NegY), Front(PosZ), Back(NegZ)
   */
  bool LoadFromFiles(VkDevice device, VkPhysicalDevice physicalDevice,
                     VkCommandPool commandPool, VkQueue graphicsQueue,
                     const std::vector<std::string> &paths);

  /**
   * @brief 从内存数据创建 CubeMap
   * @param pixels 包含 6 个面的像素数据指针数组
   */
  bool CreateFromData(VkDevice device, VkPhysicalDevice physicalDevice,
                      VkCommandPool commandPool, VkQueue graphicsQueue,
                      const std::vector<unsigned char *> &pixels,
                      uint32_t width, uint32_t height, uint32_t channels = 4);

  /**
   * @brief 生成预过滤环境贴图 (CPU重要性采样, 6 mip levels)
   */
  bool GeneratePrefilteredMap(VkDevice device, VkPhysicalDevice physicalDevice,
                              VkCommandPool commandPool, VkQueue graphicsQueue);

  /**
   * @brief 销毁 Vulkan 资源
   */
  void Destroy(VkDevice device);

private:
  bool CreateImage(VkDevice device, VkPhysicalDevice physicalDevice,
                   uint32_t width, uint32_t height, uint32_t mipLevels,
                   VkFormat format, VkImageTiling tiling,
                   VkImageUsageFlags usage, VkMemoryPropertyFlags properties);

  bool CreateImageView(VkDevice device, VkFormat format);

  bool CreateSampler(VkDevice device, VkPhysicalDevice physicalDevice);

  void TransitionImageLayout(VkDevice device, VkCommandPool commandPool,
                             VkQueue graphicsQueue, VkImageLayout oldLayout,
                             VkImageLayout newLayout);

  void CopyBufferToImage(VkDevice device, VkCommandPool commandPool,
                         VkQueue graphicsQueue, VkBuffer buffer);

  void GenerateMipmaps(VkDevice device, VkPhysicalDevice physicalDevice,
                       VkCommandPool commandPool, VkQueue graphicsQueue,
                       VkFormat format);

  static uint32_t FindMemoryType(VkPhysicalDevice physicalDevice,
                                 uint32_t typeFilter,
                                 VkMemoryPropertyFlags properties);
};

} // namespace neurender
