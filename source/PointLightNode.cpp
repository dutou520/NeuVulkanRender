#include "Nodes/PointLightNode.h"

namespace neurender {

PointLightNode::PointLightNode() : LightNode("Point Light") {}

PointLightNode::PointLightNode(const std::string &name) : LightNode(name) {}

nlohmann::json PointLightNode::ToJson() const {
  nlohmann::json j = LightNode::ToJson();
  j["nodeType"] = "PointLightNode";
  j["lightType"] = "Point";
  j["radius"] = m_Radius;
  j["constantAttenuation"] = m_ConstantAttenuation;
  j["linearAttenuation"] = m_LinearAttenuation;
  j["quadraticAttenuation"] = m_QuadraticAttenuation;
  return j;
}

std::unique_ptr<PointLightNode>
PointLightNode::FromJson(const nlohmann::json &j) {
  auto pointLight = std::make_unique<PointLightNode>();

  // 读取 Node 基础属性
  if (j.contains("name")) {
    pointLight->SetName(j["name"].get<std::string>());
  }
  if (j.contains("active")) {
    pointLight->SetActive(j["active"].get<bool>());
  }
  if (j.contains("position")) {
    pointLight->SetPosition(
        glm::vec3(j["position"][0], j["position"][1], j["position"][2]));
  }
  if (j.contains("rotation")) {
    pointLight->SetRotation(
        glm::vec3(j["rotation"][0], j["rotation"][1], j["rotation"][2]));
  }
  if (j.contains("scale")) {
    pointLight->SetScale(
        glm::vec3(j["scale"][0], j["scale"][1], j["scale"][2]));
  }

  // 读取灯光属性
  if (j.contains("color")) {
    pointLight->SetColor(
        glm::vec3(j["color"][0], j["color"][1], j["color"][2]));
  }
  if (j.contains("intensity")) {
    pointLight->SetIntensity(j["intensity"].get<float>());
  }

  // 读取点光源属性
  if (j.contains("radius")) {
    pointLight->SetRadius(j["radius"].get<float>());
  }
  if (j.contains("constantAttenuation")) {
    pointLight->SetConstantAttenuation(j["constantAttenuation"].get<float>());
  }
  if (j.contains("linearAttenuation")) {
    pointLight->SetLinearAttenuation(j["linearAttenuation"].get<float>());
  }
  if (j.contains("quadraticAttenuation")) {
    pointLight->SetQuadraticAttenuation(j["quadraticAttenuation"].get<float>());
  }

  // 子节点
  if (j.contains("children") && j["children"].is_array()) {
    for (const auto &childJson : j["children"]) {
      auto child = Node::FromJson(childJson);
      if (child) {
        pointLight->AddChild(std::move(child));
      }
    }
  }

  // UUID
  if (j.contains("uuid")) {
    pointLight->m_UUID = UUID(j["uuid"].get<std::string>());
  }

  pointLight->MarkDirty();
  return pointLight;
}

} // namespace neurender
