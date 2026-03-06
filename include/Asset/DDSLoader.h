#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace neurender {

/**
 * @brief 轻量级 DDS 加载器
 * 目前仅支持读取未压缩的 R16G16_SFLOAT (DXGI_FORMAT_R16G16_FLOAT) 等基础格式
 */
class DDSLoader {
public:
  struct DDSImage {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipMapCount = 1;
    VkFormat format = VK_FORMAT_UNDEFINED;
    std::vector<unsigned char> data;
  };

  /**
   * @brief 加载 DDS 文件
   * @param path DDS 文件路径
   * @param outImage 输出的 DDS 图像数据
   * @return 加载成功返回 true
   */
  static bool Load(const std::string &path, DDSImage &outImage);
};

} // namespace neurender
