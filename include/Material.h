#pragma once

#include "Core/UUID.h"
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
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
 * 支持纹理贴图引用
 */
struct Material {
  std::string name = "Default";

  // 材质类型
  MaterialType type = MaterialType::Opaque;

  // ========== 纹理引用 (UUID - 空表示无纹理) ==========
  UUID baseColorTexture; // 基础颜色贴图
  UUID metallicTexture;  // 金属度贴图 (B/R channel depending on convention)
  UUID roughnessTexture; // 粗糙度贴图 (G channel)
  UUID normalTexture;    // 法线贴图
  UUID emissiveTexture;  // 自发光贴图
  UUID occlusionTexture; // 环境遮蔽贴图

  // ========== PBR 因子 (与纹理相乘) ==========
  glm::vec4 baseColorFactor = glm::vec4(1.0f); // 基础颜色因子 (RGBA)
  float metallicFactor = 0.0f;                 // 金属度因子 [0, 1]
  float roughnessFactor = 1.0f;                // 粗糙度因子 [0, 1]
  float normalScale = 0.0f;                    // 法线贴图强度
  float occlusionStrength = 1.0f;              // AO 强度

  // ========== 兼容属性 (无纹理时使用) ==========
  glm::vec3 albedo = glm::vec3(1.0f, 1.0f, 1.0f); // 反照率颜色
  float metallic = 0.0f;                          // 金属度 [0, 1]
  float roughness = 1.0f;                         // 粗糙度 [0, 1]

  // 透明度 (仅 Transparent 类型使用)

  // ========== PBR 属性 ==========
  // Consolidating all PBR related members here
  float emissiveIntensity = 0.0f;
  float alpha = 1.0f;
  float shadingId = 0.0f; // 0=Lit, 1=Unlit
  uint32_t textureFlags =
      0; // 位掩码: 1=Base, 2=Metallic, 4=Norm, 8=Emiss, 16=Occ, 32=Roughness

  // 自发光属性 (所有材质支持，默认不发光)
  glm::vec3 emissiveColor = glm::vec3(1.0f); // 自发光颜色

  // 双面渲染
  bool doubleSided = false;

  // Alpha 模式: "OPAQUE", "MASK", "BLEND"
  std::string alphaMode = "OPAQUE";
  float alphaCutoff = 0.5f;

  // ========== 创建方法 ==========

  /**
   * @brief 创建默认材质 (纯白、金属度0、粗糙度1.0)
   */
  static Material CreateDefault() {
    Material mat;
    mat.name = "Default";
    mat.type = MaterialType::Opaque;
    mat.baseColorFactor = glm::vec4(1.0f);
    mat.metallicFactor = 0.0f;
    mat.roughnessFactor = 1.0f;
    return mat;
  }

  /**
   * @brief 创建预设的不透明PBR材质
   */
  static Material CreateOpaque(const glm::vec3 &color, float roughness = 0.5f,
                               float metallic = 0.0f) {
    Material mat;
    mat.name = "Opaque";
    mat.type = MaterialType::Opaque;
    mat.albedo = color;
    mat.baseColorFactor = glm::vec4(color, 1.0f);
    mat.roughness = roughness;
    mat.roughnessFactor = roughness;
    mat.metallic = metallic;
    mat.metallicFactor = metallic;
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
    mat.baseColorFactor = glm::vec4(color, alpha);
    mat.alpha = alpha;
    mat.roughness = roughness;
    mat.roughnessFactor = roughness;
    mat.metallic = 0.0f;
    mat.metallicFactor = 0.0f;
    mat.shadingId = 0.0f;
    mat.alphaMode = "BLEND";
    return mat;
  }

  /**
   * @brief 检查是否透明
   */
  bool IsTransparent() const { return type == MaterialType::Transparent; }

  /**
   * @brief 检查是否有自发光
   */
  bool HasEmission() const { return emissiveIntensity > 0.001f; }

  /**
   * @brief 检查是否有任何纹理
   */
  bool HasTextures() const {
    return !baseColorTexture.IsEmpty() || !metallicTexture.IsEmpty() ||
           !roughnessTexture.IsEmpty() || !normalTexture.IsEmpty() ||
           !emissiveTexture.IsEmpty() || !occlusionTexture.IsEmpty();
  }

  /**
   * @brief 序列化为 JSON
   */
  nlohmann::json ToJson() const {
    nlohmann::json j;
    j["name"] = name;
    j["type"] = type == MaterialType::Opaque ? "Opaque" : "Transparent";
    j["doubleSided"] = doubleSided;
    j["alphaMode"] = alphaMode;
    j["alphaCutoff"] = alphaCutoff;

    // 纹理引用
    if (!baseColorTexture.IsEmpty())
      j["baseColorTexture"] = baseColorTexture.ToString();
    if (!metallicTexture.IsEmpty())
      j["metallicTexture"] = metallicTexture.ToString();
    if (!roughnessTexture.IsEmpty())
      j["roughnessTexture"] = roughnessTexture.ToString();
    if (!normalTexture.IsEmpty())
      j["normalTexture"] = normalTexture.ToString();
    if (!emissiveTexture.IsEmpty())
      j["emissiveTexture"] = emissiveTexture.ToString();
    if (!occlusionTexture.IsEmpty())
      j["occlusionTexture"] = occlusionTexture.ToString();

    // PBR 因子
    j["baseColorFactor"] = {baseColorFactor.r, baseColorFactor.g,
                            baseColorFactor.b, baseColorFactor.a};
    j["metallicFactor"] = metallicFactor;
    j["roughnessFactor"] = roughnessFactor;
    j["normalScale"] = normalScale;
    j["occlusionStrength"] = occlusionStrength;

    // 自发光
    j["emissiveColor"] = {emissiveColor.r, emissiveColor.g, emissiveColor.b};
    j["emissiveIntensity"] = emissiveIntensity;

    j["shadingId"] = shadingId;

    return j;
  }

  /**
   * @brief 从 JSON 反序列化
   */
  static Material FromJson(const nlohmann::json &j) {
    Material mat;

    if (j.contains("name"))
      mat.name = j["name"].get<std::string>();

    if (j.contains("type")) {
      std::string typeStr = j["type"].get<std::string>();
      mat.type = (typeStr == "Transparent") ? MaterialType::Transparent
                                            : MaterialType::Opaque;
    }

    if (j.contains("doubleSided"))
      mat.doubleSided = j["doubleSided"].get<bool>();
    if (j.contains("alphaMode"))
      mat.alphaMode = j["alphaMode"].get<std::string>();
    if (j.contains("alphaCutoff"))
      mat.alphaCutoff = j["alphaCutoff"].get<float>();

    // 纹理引用
    if (j.contains("baseColorTexture"))
      mat.baseColorTexture =
          UUID::FromString(j["baseColorTexture"].get<std::string>());
    if (j.contains("metallicTexture"))
      mat.metallicTexture =
          UUID::FromString(j["metallicTexture"].get<std::string>());
    if (j.contains("roughnessTexture"))
      mat.roughnessTexture =
          UUID::FromString(j["roughnessTexture"].get<std::string>());
    if (j.contains("normalTexture"))
      mat.normalTexture =
          UUID::FromString(j["normalTexture"].get<std::string>());
    if (j.contains("emissiveTexture"))
      mat.emissiveTexture =
          UUID::FromString(j["emissiveTexture"].get<std::string>());
    if (j.contains("occlusionTexture"))
      mat.occlusionTexture =
          UUID::FromString(j["occlusionTexture"].get<std::string>());

    // PBR 因子
    if (j.contains("baseColorFactor")) {
      auto &arr = j["baseColorFactor"];
      mat.baseColorFactor = glm::vec4(arr[0].get<float>(), arr[1].get<float>(),
                                      arr[2].get<float>(), arr[3].get<float>());
      mat.albedo = glm::vec3(mat.baseColorFactor);
      mat.alpha = mat.baseColorFactor.a;
    }
    if (j.contains("metallicFactor")) {
      mat.metallicFactor = j["metallicFactor"].get<float>();
      mat.metallic = mat.metallicFactor;
    }
    if (j.contains("roughnessFactor")) {
      mat.roughnessFactor = j["roughnessFactor"].get<float>();
      mat.roughness = mat.roughnessFactor;
    }
    if (j.contains("normalScale"))
      mat.normalScale = j["normalScale"].get<float>();
    if (j.contains("occlusionStrength"))
      mat.occlusionStrength = j["occlusionStrength"].get<float>();

    // 自发光
    if (j.contains("emissiveColor")) {
      auto &arr = j["emissiveColor"];
      mat.emissiveColor = glm::vec3(arr[0].get<float>(), arr[1].get<float>(),
                                    arr[2].get<float>());
    }
    if (j.contains("emissiveIntensity"))
      mat.emissiveIntensity = j["emissiveIntensity"].get<float>();

    if (j.contains("shadingId"))
      mat.shadingId = j["shadingId"].get<float>();

    return mat;
  }
};

} // namespace neurender
