#pragma once
#include "Nodes/LightNode.h"

namespace neurender {

/**
 * @brief PointLightNode 点光源节点
 * 具有位置、半径和衰减参数的点光源
 */
class PointLightNode : public LightNode {
public:
  PointLightNode();
  explicit PointLightNode(const std::string &name);
  ~PointLightNode() override = default;

  // ========== 点光源属性 ==========
  float GetRadius() const { return m_Radius; }
  float GetConstantAttenuation() const { return m_ConstantAttenuation; }
  float GetLinearAttenuation() const { return m_LinearAttenuation; }
  float GetQuadraticAttenuation() const { return m_QuadraticAttenuation; }

  void SetRadius(float radius) { m_Radius = radius; }
  void SetConstantAttenuation(float constant) {
    m_ConstantAttenuation = constant;
  }
  void SetLinearAttenuation(float linear) { m_LinearAttenuation = linear; }
  void SetQuadraticAttenuation(float quadratic) {
    m_QuadraticAttenuation = quadratic;
  }

  // ========== 类型标识 ==========
  std::string GetNodeType() const override { return "PointLightNode"; }
  std::string GetLightType() const override { return "Point"; }

  // ========== 序列化 ==========
  nlohmann::json ToJson() const override;
  static std::unique_ptr<PointLightNode> FromJson(const nlohmann::json &j);

private:
  float m_Radius{10.0f};                // 光照影响半径
  float m_ConstantAttenuation{1.0f};    // 常数衰减系数
  float m_LinearAttenuation{0.09f};     // 线性衰减系数
  float m_QuadraticAttenuation{0.032f}; // 二次衰减系数
};

} // namespace neurender
