#pragma once

#include "Core/UUID.h"
#include <string>
#include <vulkan/vulkan.h>

namespace neurender {

/**
 * @brief 纹理资源类
 * 封装 Vulkan 纹理资源 (VkImage, VkImageView, VkSampler)
 */
struct TextureResource {
  UUID uuid;
  std::string name;
  std::string filePath;

  // Vulkan 资源句柄
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView imageView = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;

  // 纹理属性
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t channels = 4;
  uint32_t mipLevels = 1;

  // 加载状态
  bool isLoaded = false;

  /**
   * @brief 从文件加载纹理
   * @param device Vulkan 逻辑设备
   * @param physicalDevice Vulkan 物理设备
   * @param commandPool 命令池
   * @param graphicsQueue 图形队列
   * @param path 纹理文件路径
   * @return 加载成功返回 true
   */
  bool LoadFromFile(VkDevice device, VkPhysicalDevice physicalDevice,
                    VkCommandPool commandPool, VkQueue graphicsQueue,
                    const std::string &path);

  /**
   * @brief 从内存数据创建纹理
   * @param device Vulkan 逻辑设备
   * @param physicalDevice Vulkan 物理设备
   * @param commandPool 命令池
   * @param graphicsQueue 图形队列
   * @param pixels 像素数据
   * @param width 宽度
   * @param height 高度
   * @param channels 通道数
   * @return 创建成功返回 true
   */
  bool CreateFromData(VkDevice device, VkPhysicalDevice physicalDevice,
                      VkCommandPool commandPool, VkQueue graphicsQueue,
                      const unsigned char *pixels, uint32_t width,
                      uint32_t height, uint32_t channels = 4);

  /**
   * @brief 销毁 Vulkan 资源
   * @param device Vulkan 逻辑设备
   */
  void Destroy(VkDevice device);

private:
  /**
   * @brief 创建 Vulkan 图像
   */
  bool CreateImage(VkDevice device, VkPhysicalDevice physicalDevice,
                   uint32_t width, uint32_t height, uint32_t mipLevels,
                   VkFormat format, VkImageTiling tiling,
                   VkImageUsageFlags usage, VkMemoryPropertyFlags properties);

  /**
   * @brief 创建图像视图
   */
  bool CreateImageView(VkDevice device, VkFormat format);

  /**
   * @brief 创建采样器
   */
  bool CreateSampler(VkDevice device, VkPhysicalDevice physicalDevice);

  /**
   * @brief 过渡图像布局
   */
  void TransitionImageLayout(VkDevice device, VkCommandPool commandPool,
                             VkQueue graphicsQueue, VkImageLayout oldLayout,
                             VkImageLayout newLayout);

  /**
   * @brief 从缓冲区复制到图像
   */
  void CopyBufferToImage(VkDevice device, VkCommandPool commandPool,
                         VkQueue graphicsQueue, VkBuffer buffer);

  /**
   * @brief 生成 Mipmap
   */
  void GenerateMipmaps(VkDevice device, VkPhysicalDevice physicalDevice,
                       VkCommandPool commandPool, VkQueue graphicsQueue,
                       VkFormat format);

  /**
   * @brief 查找内存类型
   */
  static uint32_t FindMemoryType(VkPhysicalDevice physicalDevice,
                                 uint32_t typeFilter,
                                 VkMemoryPropertyFlags properties);
};

} // namespace neurender
