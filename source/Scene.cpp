#include "Scene/Scene.h"
#include "Nodes/Node.h"
#include "neuLog.h"
#include <filesystem>
#include <fstream>

namespace neurender {

Scene::Scene()
    : Object("New Scene"), m_RootNode(std::make_unique<Node>("Root")) {}

Scene::Scene(const std::string &name)
    : Object(name), m_RootNode(std::make_unique<Node>("Root")) {}

Scene::~Scene() = default;

std::shared_ptr<Scene> Scene::Create(const std::string &name) {
  return std::make_shared<Scene>(name);
}

std::shared_ptr<Scene> Scene::Load(const std::string &filePath) {
  std::ifstream file(std::filesystem::u8path(filePath));
  if (!file.is_open()) {
    LOG_E("Failed to open scene file: {}", filePath);
    return nullptr;
  }

  try {
    nlohmann::json j;
    file >> j;
    auto scene = FromJson(j);
    if (scene) {
      LOG_I("Loaded scene: {} from {}", scene->GetName(), filePath);
    }
    return scene;
  } catch (const std::exception &e) {
    LOG_E("Failed to parse scene file: {}", e.what());
    return nullptr;
  }
}

bool Scene::Save(const std::string &filePath) const {
  // Create parent directories if they don't exist
  std::filesystem::path p = std::filesystem::u8path(filePath);
  if (p.has_parent_path()) {
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    if (ec) {
      LOG_E("Failed to create directory {}: {}", p.parent_path().u8string(),
            ec.message());
      return false;
    }
  }

  std::ofstream file(p);
  if (!file.is_open()) {
    LOG_E("Failed to save scene file: {}", filePath);
    return false;
  }

  file << ToJson().dump(2);
  LOG_I("Saved scene: {} to {}", m_Name, filePath);
  return true;
}

void Scene::AddNode(std::unique_ptr<Node> node) {
  if (m_RootNode && node) {
    m_RootNode->AddChild(std::move(node));
  }
}

Node *Scene::FindNode(const UUID &uuid) const {
  return FindNodeRecursive(m_RootNode.get(), uuid);
}

Node *Scene::FindNode(const std::string &name) const {
  return FindNodeRecursive(m_RootNode.get(), name);
}

Node *Scene::FindNodeRecursive(Node *node, const UUID &uuid) const {
  if (!node)
    return nullptr;

  if (node->GetUUID() == uuid) {
    return node;
  }

  for (const auto &child : node->GetChildren()) {
    Node *found = FindNodeRecursive(child.get(), uuid);
    if (found)
      return found;
  }

  return nullptr;
}

Node *Scene::FindNodeRecursive(Node *node, const std::string &name) const {
  if (!node)
    return nullptr;

  if (node->GetName() == name) {
    return node;
  }

  for (const auto &child : node->GetChildren()) {
    Node *found = FindNodeRecursive(child.get(), name);
    if (found)
      return found;
  }

  return nullptr;
}

void Scene::TraverseNodes(std::function<void(Node *)> callback) {
  TraverseNodesRecursive(m_RootNode.get(), callback);
}

void Scene::TraverseNodesRecursive(Node *node,
                                   std::function<void(Node *)> &callback) {
  if (!node)
    return;

  callback(node);

  for (const auto &child : node->GetChildren()) {
    TraverseNodesRecursive(child.get(), callback);
  }
}

nlohmann::json Scene::ToJson() const {
  nlohmann::json j;
  j["uuid"] = m_UUID.ToString();
  j["name"] = m_Name;
  j["rootNode"] = m_RootNode ? m_RootNode->ToJson() : nlohmann::json{};
  return j;
}

std::shared_ptr<Scene> Scene::FromJson(const nlohmann::json &j) {
  auto scene = std::make_shared<Scene>();
  scene->m_Name = j.value("name", "Unnamed Scene");

  if (j.contains("rootNode") && !j["rootNode"].is_null()) {
    scene->m_RootNode = Node::FromJson(j["rootNode"]);
  }

  return scene;
}

} // namespace neurender
