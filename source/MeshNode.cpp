#include "Nodes/MeshNode.h"

namespace neurender {

MeshNode::MeshNode()
    : Node("MeshNode"), m_MeshID(UUID::Invalid()),
      m_MaterialID(UUID::Invalid()) {}

MeshNode::MeshNode(const std::string &name)
    : Node(name), m_MeshID(UUID::Invalid()), m_MaterialID(UUID::Invalid()) {}

void MeshNode::CollectRenderables(SceneRenderer &renderer,
                                  const glm::mat4 &parentMatrix) {
  if (!m_IsActive)
    return;

  glm::mat4 globalMatrix = parentMatrix * GetLocalMatrix();

  // TODO: 将渲染数据提交给渲染器
  // renderer.SubmitMesh(m_MeshID, m_MaterialID, globalMatrix);

  // 递归处理子节点
  for (auto &child : m_Children) {
    child->CollectRenderables(renderer, globalMatrix);
  }
}

nlohmann::json MeshNode::ToJson() const {
  nlohmann::json j = Node::ToJson();
  j["type"] = GetNodeType();
  j["meshID"] = m_MeshID.ToString();
  j["materialID"] = m_MaterialID.ToString();
  return j;
}

std::unique_ptr<MeshNode> MeshNode::FromJson(const nlohmann::json &j) {
  auto node = std::make_unique<MeshNode>();

  node->m_Name = j.value("name", "MeshNode");
  node->m_IsActive = j.value("active", true);

  // Transform
  if (j.contains("position") && j["position"].is_array()) {
    node->m_Position =
        glm::vec3(j["position"][0].get<float>(), j["position"][1].get<float>(),
                  j["position"][2].get<float>());
  }
  if (j.contains("rotation") && j["rotation"].is_array()) {
    node->m_Rotation =
        glm::vec3(j["rotation"][0].get<float>(), j["rotation"][1].get<float>(),
                  j["rotation"][2].get<float>());
  }
  if (j.contains("scale") && j["scale"].is_array()) {
    node->m_Scale =
        glm::vec3(j["scale"][0].get<float>(), j["scale"][1].get<float>(),
                  j["scale"][2].get<float>());
  }

  // 资源引用
  node->m_MeshID = UUID(j.value("meshID", "0"));
  node->m_MaterialID = UUID(j.value("materialID", "0"));

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
