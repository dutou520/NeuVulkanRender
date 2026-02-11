#pragma once
#include "Nodes/Node.h"

namespace neurender {

/**
 * @brief CameraNode 虚拟相机节点
 * 可以作为场景中的对象，并允许对齐视角
 */
class CameraNode : public Node {
public:
  CameraNode();
  explicit CameraNode(const std::string &name);
  ~CameraNode() override = default;

  // 相机属性
  float GetFov() const { return m_Fov; }
  void SetFov(float fov) { m_Fov = fov; }

  float GetNearPlane() const { return m_NearPlane; }
  void SetNearPlane(float near) { m_NearPlane = near; }

  float GetFarPlane() const { return m_FarPlane; }
  void SetFarPlane(float far) { m_FarPlane = far; }

  float GetMovementSpeed() const { return m_MovementSpeed; }
  void SetMovementSpeed(float speed) { m_MovementSpeed = speed; }

  float GetMouseSensitivity() const { return m_MouseSensitivity; }
  void SetMouseSensitivity(float sensitivity) {
    m_MouseSensitivity = sensitivity;
  }

  // 序列化
  nlohmann::json ToJson() const override;
  static std::unique_ptr<CameraNode> FromJson(const nlohmann::json &j);

  std::string GetNodeType() const override { return "CameraNode"; }

private:
  float m_MovementSpeed = 2.5f;
  float m_MouseSensitivity = 0.1f;
  float m_Fov = 45.0f;
  float m_NearPlane = 0.1f;
  float m_FarPlane = 100.0f;
};

} // namespace neurender
