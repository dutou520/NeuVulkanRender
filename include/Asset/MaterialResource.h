#pragma once

#include "Asset/TextureResource.h"
#include "Core/UUID.h"
#include "Material.h"
#include <string>
#include <vulkan/vulkan.h>

namespace neurender {

/**
 * @brief 材质资源类
 * 运行时材质资源，包含加载的纹理引用和描述符集
 */
struct MaterialResource {
  UUID uuid;
  std::string name;
  std::string filePath;

  std::string GetName() const { return name; }
  void SetName(const std::string &n) { name = n; }

  // 材质属性
  Material material;

  // 加载的纹理指针 (缓存引用)
  TextureResource *baseColorTex = nullptr;
  TextureResource *metallicTex = nullptr;
  TextureResource *roughnessTex = nullptr;
  TextureResource *normalTex = nullptr;
  TextureResource *emissiveTex = nullptr;
  TextureResource *occlusionTex = nullptr;

  // Vulkan 描述符集
  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

  // 加载状态
  bool isLoaded = false;

  /**
   * @brief 从 .mat.json 文件加载材质
   * @param path 材质文件路径
   * @return 加载成功返回 true
   */
  bool LoadFromFile(const std::string &path);

  /**
   * @brief 保存材质到 .mat.json 文件
   * @param path 保存路径
   * @return 保存成功返回 true
   */
  bool SaveToFile(const std::string &path) const;

  /**
   * @brief 检查是否有任何纹理
   */
  bool HasTextures() const;

  /**
   * @brief 获取使用的纹理标志 (用于 shader)
   * @return bit0=baseColor, bit1=metallic, bit2=normal, bit3=emissive,
   * bit4=occlusion, bit5=roughness
   */
  uint32_t GetTextureFlags() const;
};

} // namespace neurender
