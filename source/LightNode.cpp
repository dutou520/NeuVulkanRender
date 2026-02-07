#include "Nodes/LightNode.h"

namespace neurender {

LightNode::LightNode() : Node("Light") {}

LightNode::LightNode(const std::string &name) : Node(name) {}

nlohmann::json LightNode::ToJson() const {
  nlohmann::json j = Node::ToJson();
  j["nodeType"] = "LightNode";
  j["lightType"] = GetLightType();
  j["color"] = {m_Color.r, m_Color.g, m_Color.b};
  j["intensity"] = m_Intensity;
  return j;
}

std::unique_ptr<LightNode> LightNode::FromJson(const nlohmann::json &j) {
  auto light = std::make_unique<LightNode>();

  // 读取 Node 基础属性
  if (j.contains("name")) {
    light->SetName(j["name"].get<std::string>());
  }
  if (j.contains("active")) {
    light->SetActive(j["active"].get<bool>());
  }
  if (j.contains("position")) {
    light->SetPosition(
        glm::vec3(j["position"][0], j["position"][1], j["position"][2]));
  }
  if (j.contains("rotation")) {
    light->SetRotation(
        glm::vec3(j["rotation"][0], j["rotation"][1], j["rotation"][2]));
  }
  if (j.contains("scale")) {
    light->SetScale(glm::vec3(j["scale"][0], j["scale"][1], j["scale"][2]));
  }

  // 读取灯光属性
  if (j.contains("color")) {
    light->SetColor(glm::vec3(j["color"][0], j["color"][1], j["color"][2]));
  }
  if (j.contains("intensity")) {
    light->SetIntensity(j["intensity"].get<float>());
  }

  // 子节点
  if (j.contains("children") && j["children"].is_array()) {
    for (const auto &childJson : j["children"]) {
      auto child = Node::FromJson(childJson);
      if (child) {
        light->AddChild(std::move(child));
      }
    }
  }

  // UUID
  if (j.contains("uuid")) {
    light->m_UUID = UUID(j["uuid"].get<std::string>());
  }

  light->MarkDirty();
  return light;
}

} // namespace neurender
