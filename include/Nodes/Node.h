#pragma once
#include "Core/Object.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <vector>

namespace neurender {

class SceneRenderer;

/**
 * @brief Node 节点基类
 * 场景图的核心，包含 Transform 和父子层级关系
 */
class Node : public Object {
public:
  Node();
  explicit Node(const std::string &name);
  virtual ~Node();

  // 禁止拷贝
  Node(const Node &) = delete;
  Node &operator=(const Node &) = delete;

  // ========== 状态 ==========
  bool IsActive() const { return m_IsActive; }
  void SetActive(bool active) { m_IsActive = active; }

  // ========== 层级关系 ==========
  Node *GetParent() const { return m_Parent; }
  const std::vector<std::unique_ptr<Node>> &GetChildren() const {
    return m_Children;
  }

  void AddChild(std::unique_ptr<Node> child);
  std::unique_ptr<Node> RemoveChild(Node *child);
  void SetParent(Node *parent);

  // ========== Transform ==========
  const glm::vec3 &GetPosition() const { return m_Position; }
  const glm::vec3 &GetRotation() const { return m_Rotation; } // 欧拉角 (度)
  const glm::vec3 &GetScale() const { return m_Scale; }

  void SetPosition(const glm::vec3 &position);
  void SetRotation(const glm::vec3 &rotation); // 欧拉角 (度)
  void SetScale(const glm::vec3 &scale);

  // 获取变换矩阵
  const glm::mat4 &GetLocalMatrix();
  const glm::mat4 &GetGlobalMatrix();

  // ========== 渲染收集 ==========
  virtual void CollectRenderables(SceneRenderer &renderer,
                                  const glm::mat4 &parentMatrix);

  // ========== 序列化 ==========
  virtual nlohmann::json ToJson() const;
  static std::unique_ptr<Node> FromJson(const nlohmann::json &j);

  // 节点类型标识
  virtual std::string GetNodeType() const { return "Node"; }

protected:
  // 标记矩阵需要更新
  void MarkDirty();
  void UpdateLocalMatrix();
  void UpdateGlobalMatrix();

  // 状态
  bool m_IsActive = true;

  // 层级
  Node *m_Parent = nullptr;
  std::vector<std::unique_ptr<Node>> m_Children;

  // Transform
  glm::vec3 m_Position{0.0f, 0.0f, 0.0f};
  glm::vec3 m_Rotation{0.0f, 0.0f, 0.0f}; // 欧拉角 (度)
  glm::vec3 m_Scale{1.0f, 1.0f, 1.0f};

  // 缓存的矩阵
  glm::mat4 m_LocalMatrix{1.0f};
  glm::mat4 m_GlobalMatrix{1.0f};
  bool m_LocalMatrixDirty = true;
  bool m_GlobalMatrixDirty = true;
};

} // namespace neurender
