#pragma once
#include "Nodes/Node.h"
#include <glm/glm.hpp>

namespace neurender {

/**
 * @brief LightNode 灯光节点基类
 * 所有灯光类型的基础，包含颜色和强度
 */
class LightNode : public Node {
public:
  LightNode();
  explicit LightNode(const std::string &name);
  ~LightNode() override = default;

  // ========== 灯光属性 ==========
  const glm::vec3 &GetColor() const { return m_Color; }
  float GetIntensity() const { return m_Intensity; }

  void SetColor(const glm::vec3 &color) { m_Color = color; }
  void SetIntensity(float intensity) { m_Intensity = intensity; }

  // ========== 类型标识 ==========
  std::string GetNodeType() const override { return "LightNode"; }
  virtual std::string GetLightType() const { return "Base"; }

  // ========== 序列化 ==========
  nlohmann::json ToJson() const override;
  static std::unique_ptr<LightNode> FromJson(const nlohmann::json &j);

protected:
  glm::vec3 m_Color{1.0f, 1.0f, 1.0f}; // 灯光颜色 (线性空间)
  float m_Intensity{1.0f};             // 灯光强度
};

} // namespace neurender
