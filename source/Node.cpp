#include "Nodes/Node.h"
#include "Nodes/CameraNode.h"
#include "Nodes/MeshNode.h"
#include "Nodes/PointLightNode.h"

namespace neurender {

Node::Node() : Object("Node") {}

Node::Node(const std::string &name) : Object(name) {}

Node::~Node() {
  // 子节点会自动销毁 (unique_ptr)
}

// ========== 层级关系 ==========

void Node::AddChild(std::unique_ptr<Node> child) {
  if (!child)
    return;

  // 从原父节点移除
  if (child->m_Parent) {
    child->m_Parent->RemoveChild(child.get());
  }

  child->m_Parent = this;
  child->MarkDirty();
  m_Children.push_back(std::move(child));
}

std::unique_ptr<Node> Node::RemoveChild(Node *child) {
  for (auto it = m_Children.begin(); it != m_Children.end(); ++it) {
    if (it->get() == child) {
      child->m_Parent = nullptr;
      child->MarkDirty();
      std::unique_ptr<Node> removed = std::move(*it);
      m_Children.erase(it);
      return removed;
    }
  }
  return nullptr;
}

void Node::SetParent(Node *parent) {
  if (m_Parent == parent)
    return;

  if (parent) {
    // 创建临时 unique_ptr 转移所有权
    if (m_Parent) {
      auto self = m_Parent->RemoveChild(this);
      parent->AddChild(std::move(self));
    }
  }
}

// ========== Transform ==========

void Node::SetPosition(const glm::vec3 &position) {
  m_Position = position;
  MarkDirty();
}

void Node::SetRotation(const glm::vec3 &rotation) {
  m_Rotation = rotation;
  MarkDirty();
}

void Node::SetScale(const glm::vec3 &scale) {
  m_Scale = scale;
  MarkDirty();
}

const glm::mat4 &Node::GetLocalMatrix() {
  if (m_LocalMatrixDirty) {
    UpdateLocalMatrix();
  }
  return m_LocalMatrix;
}

const glm::mat4 &Node::GetGlobalMatrix() {
  if (m_GlobalMatrixDirty) {
    UpdateGlobalMatrix();
  }
  return m_GlobalMatrix;
}

void Node::MarkDirty() {
  m_LocalMatrixDirty = true;
  m_GlobalMatrixDirty = true;

  // 子节点的全局矩阵也需要更新
  for (auto &child : m_Children) {
    child->m_GlobalMatrixDirty = true;
    child->MarkDirty();
  }
}

void Node::UpdateLocalMatrix() {
  m_LocalMatrix = glm::mat4(1.0f);
  m_LocalMatrix = glm::translate(m_LocalMatrix, m_Position);

  // 旋转顺序: Y -> X -> Z (常用的 YXZ 顺序)
  m_LocalMatrix = glm::rotate(m_LocalMatrix, glm::radians(m_Rotation.y),
                              glm::vec3(0, 1, 0));
  m_LocalMatrix = glm::rotate(m_LocalMatrix, glm::radians(m_Rotation.x),
                              glm::vec3(1, 0, 0));
  m_LocalMatrix = glm::rotate(m_LocalMatrix, glm::radians(m_Rotation.z),
                              glm::vec3(0, 0, 1));

  m_LocalMatrix = glm::scale(m_LocalMatrix, m_Scale);

  m_LocalMatrixDirty = false;
}

void Node::UpdateGlobalMatrix() {
  if (m_LocalMatrixDirty) {
    UpdateLocalMatrix();
  }

  if (m_Parent) {
    m_GlobalMatrix = m_Parent->GetGlobalMatrix() * m_LocalMatrix;
  } else {
    m_GlobalMatrix = m_LocalMatrix;
  }

  m_GlobalMatrixDirty = false;
}

// ========== 渲染收集 ==========

void Node::CollectRenderables(SceneRenderer &renderer,
                              const glm::mat4 &parentMatrix) {
  if (!m_IsActive)
    return;

  // 计算当前节点的全局矩阵
  glm::mat4 globalMatrix = parentMatrix * GetLocalMatrix();

  // 子类重写此方法以提交渲染数据

  // 递归处理子节点
  for (auto &child : m_Children) {
    child->CollectRenderables(renderer, globalMatrix);
  }
}

// ========== 序列化 ==========

nlohmann::json Node::ToJson() const {
  nlohmann::json j;
  j["type"] = GetNodeType();
  j["uuid"] = m_UUID.ToString();
  j["name"] = m_Name;
  j["active"] = m_IsActive;

  // Transform
  j["position"] = {m_Position.x, m_Position.y, m_Position.z};
  j["rotation"] = {m_Rotation.x, m_Rotation.y, m_Rotation.z};
  j["scale"] = {m_Scale.x, m_Scale.y, m_Scale.z};

  // 子节点
  nlohmann::json children = nlohmann::json::array();
  for (const auto &child : m_Children) {
    children.push_back(child->ToJson());
  }
  j["children"] = children;

  return j;
}

std::unique_ptr<Node> Node::FromJson(const nlohmann::json &j) {
  std::string type = j.value("type", "Node");

  // 根据类型创建不同的节点
  if (type == "MeshNode") {
    return MeshNode::FromJson(j);
  } else if (type == "PointLightNode") {
    return PointLightNode::FromJson(j);
  } else if (type == "CameraNode") {
    return CameraNode::FromJson(j);
  }

  // 基础 Node逻辑
  auto node = std::make_unique<Node>();

  node->m_UUID = UUID(j.value("uuid", UUID::Generate().ToString()));
  node->m_Name = j.value("name", "Node");
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

std::unique_ptr<Node> Node::Clone() const {
  nlohmann::json j = ToJson();
  auto copy = FromJson(j);
  if (copy) {
    copy->GenerateNewUUIDs();
    copy->SetName(GetName() + " (Copy)");
  }
  return copy;
}

void Node::GenerateNewUUIDs() {
  m_UUID = UUID::Generate();
  for (auto &child : m_Children) {
    child->GenerateNewUUIDs();
  }
}

} // namespace neurender
