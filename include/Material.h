#pragma once

#include <glm/glm.hpp>
#include <string>

namespace neurender {

/**
 * @brief 材质类型枚举
 */
enum class MaterialType {
  Opaque,     // 不透明材质 - 使用延迟渲染
  Transparent // 透明材质 - 使用前向渲染
};

/**
 * @brief 材质类
 *
 * 支持两种材质类型：不透明和透明
 * 所有材质均支持自发光（默认亮度为0）
 */
struct Material {
  std::string name = "Default";

  // 材质类型
  MaterialType type = MaterialType::Opaque;

  // PBR 属性
  glm::vec3 albedo = glm::vec3(0.8f, 0.8f, 0.8f); // 反照率颜色
  float metallic = 0.0f;                          // 金属度 [0, 1]
  float roughness = 0.5f;                         // 粗糙度 [0, 1]

  // 透明度 (仅 Transparent 类型使用)
  float alpha = 1.0f; // [0, 1]

  // 自发光属性 (所有材质支持，默认不发光)
  glm::vec3 emissiveColor = glm::vec3(1.0f); // 自发光颜色
  float emissiveIntensity = 0.0f;            // 自发光强度，默认0不发光

  // 着色模型 ID
  // 0-100: PBR
  // 101-200: 卡通着色
  // 201-255: 纯自发光
  float shadingId = 0.0f;

  /**
   * @brief 创建预设的不透明PBR材质
   */
  static Material CreateOpaque(const glm::vec3 &color, float roughness = 0.5f,
                               float metallic = 0.0f) {
    Material mat;
    mat.name = "Opaque";
    mat.type = MaterialType::Opaque;
    mat.albedo = color;
    mat.roughness = roughness;
    mat.metallic = metallic;
    mat.shadingId = 0.0f;
    return mat;
  }

  /**
   * @brief 创建预设的透明材质
   */
  static Material CreateTransparent(const glm::vec3 &color, float alpha,
                                    float roughness = 0.5f) {
    Material mat;
    mat.name = "Transparent";
    mat.type = MaterialType::Transparent;
    mat.albedo = color;
    mat.alpha = alpha;
    mat.roughness = roughness;
    mat.metallic = 0.0f;
    mat.shadingId = 0.0f;
    return mat;
  }

  /**
   * @brief 创建预设的自发光材质
   */
  static Material CreateEmissive(const glm::vec3 &color,
                                 float intensity = 1.0f) {
    Material mat;
    mat.name = "Emissive";
    mat.type = MaterialType::Opaque;
    mat.albedo = color;
    mat.emissiveColor = color;
    mat.emissiveIntensity = intensity;
    mat.shadingId = 201.0f; // 纯自发光着色模型
    return mat;
  }

  /**
   * @brief 设置自发光
   */
  void SetEmissive(const glm::vec3 &color, float intensity) {
    emissiveColor = color;
    emissiveIntensity = intensity;
  }

  /**
   * @brief 检查是否透明
   */
  bool IsTransparent() const { return type == MaterialType::Transparent; }

  /**
   * @brief 检查是否有自发光
   */
  bool HasEmission() const { return emissiveIntensity > 0.001f; }
};

} // namespace neurender
