#pragma once
#include "Core/Object.h"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace neurender {

class Node;

/**
 * @brief Scene 类
 * 管理场景节点树，支持 JSON 序列化
 */
class Scene : public Object {
public:
  Scene();
  explicit Scene(const std::string &name);
  ~Scene();

  // 场景操作
  static std::shared_ptr<Scene> Create(const std::string &name);
  static std::shared_ptr<Scene> Load(const std::string &filePath);
  bool Save(const std::string &filePath) const;

  // 节点管理
  Node *GetRootNode() const { return m_RootNode.get(); }
  Node *FindNode(const UUID &uuid) const;
  Node *FindNode(const std::string &name) const;

  // 添加节点到根节点
  void AddNode(std::unique_ptr<Node> node);

  // 遍历所有节点
  void TraverseNodes(std::function<void(Node *)> callback);

  // JSON 序列化
  nlohmann::json ToJson() const;
  static std::shared_ptr<Scene> FromJson(const nlohmann::json &j);

private:
  std::unique_ptr<Node> m_RootNode; // 虚拟根节点

  // 递归查找节点
  Node *FindNodeRecursive(Node *node, const UUID &uuid) const;
  Node *FindNodeRecursive(Node *node, const std::string &name) const;
  void TraverseNodesRecursive(Node *node,
                              std::function<void(Node *)> &callback);
};

} // namespace neurender
