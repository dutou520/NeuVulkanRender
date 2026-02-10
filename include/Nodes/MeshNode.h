#pragma once
#include "Core/UUID.h"
#include "Nodes/Node.h"

namespace neurender {

/**
 * @brief MeshNode 网格节点
 * 引用 Mesh 和 Material 资源
 */
class MeshNode : public Node {
public:
  MeshNode();
  explicit MeshNode(const std::string &name);
  ~MeshNode() override = default;

  // 资源引用
  const UUID &GetMeshID() const { return m_MeshID; }
  const UUID &GetMaterialID() const { return m_MaterialID; }

  void SetMeshID(const UUID &meshID) { m_MeshID = meshID; }
  void SetMaterialID(const UUID &materialID) { m_MaterialID = materialID; }

  // 渲染收集 (将 mesh、material 和 globalMatrix 打包提交给渲染器)
  void CollectRenderables(SceneRenderer &renderer,
                          const glm::mat4 &parentMatrix) override;

  // 序列化
  nlohmann::json ToJson() const override;
  static std::unique_ptr<MeshNode> FromJson(const nlohmann::json &j);

  std::string GetNodeType() const override { return "MeshNode"; }

private:
  UUID m_MeshID;     // Mesh 资源 GUID
  UUID m_MaterialID; // Material 资源 GUID
};

} // namespace neurender
