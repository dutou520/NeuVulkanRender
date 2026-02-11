#include "Nodes/CameraNode.h"

namespace neurender {

CameraNode::CameraNode() : Node("Camera") {}

CameraNode::CameraNode(const std::string &name) : Node(name) {}

nlohmann::json CameraNode::ToJson() const {
  nlohmann::json j = Node::ToJson();
  j["movementSpeed"] = m_MovementSpeed;
  j["mouseSensitivity"] = m_MouseSensitivity;
  j["fov"] = m_Fov;
  j["nearPlane"] = m_NearPlane;
  j["farPlane"] = m_FarPlane;
  return j;
}

std::unique_ptr<CameraNode> CameraNode::FromJson(const nlohmann::json &j) {
  auto node = std::make_unique<CameraNode>();

  // 加载基础属性
  if (j.contains("uuid"))
    node->m_UUID = UUID(j["uuid"].get<std::string>());
  if (j.contains("name"))
    node->m_Name = j["name"].get<std::string>();
  if (j.contains("active"))
    node->m_IsActive = j["active"].get<bool>();

  // Transform
  if (j.contains("position") && j["position"].is_array()) {
    node->m_Position =
        glm::vec3(j["position"][0], j["position"][1], j["position"][2]);
  }
  if (j.contains("rotation") && j["rotation"].is_array()) {
    node->m_Rotation =
        glm::vec3(j["rotation"][0], j["rotation"][1], j["rotation"][2]);
  }
  if (j.contains("scale") && j["scale"].is_array()) {
    node->m_Scale = glm::vec3(j["scale"][0], j["scale"][1], j["scale"][2]);
  }

  // Camera properties
  node->m_MovementSpeed = j.value("movementSpeed", 2.5f);
  node->m_MouseSensitivity = j.value("mouseSensitivity", 0.1f);
  node->m_Fov = j.value("fov", 45.0f);
  node->m_NearPlane = j.value("nearPlane", 0.1f);
  node->m_FarPlane = j.value("farPlane", 100.0f);

  // 子节点
  if (j.contains("children") && j["children"].is_array()) {
    for (const auto &childJson : j["children"]) {
      auto child = Node::FromJson(childJson);
      if (child) {
        node->AddChild(std::move(child));
      }
    }
  }

  node->MarkDirty();
  return node;
}

} // namespace neurender
