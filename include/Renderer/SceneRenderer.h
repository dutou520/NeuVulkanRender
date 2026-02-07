#pragma once
#include "Core/UUID.h"
#include <glm/glm.hpp>
#include <vector>

namespace neurender {

struct RenderCommand {
  UUID meshID;
  UUID materialID;
  glm::mat4 transform;
};

/**
 * @brief SceneRenderer 场景渲染器接口
 * 用于节点收集渲染数据
 */
class SceneRenderer {
public:
  void SubmitMesh(const UUID &meshID, const UUID &materialID,
                  const glm::mat4 &transform) {
    m_RenderCommands.push_back({meshID, materialID, transform});
  }

  const std::vector<RenderCommand> &GetRenderCommands() const {
    return m_RenderCommands;
  }
  void Clear() { m_RenderCommands.clear(); }

private:
  std::vector<RenderCommand> m_RenderCommands;
};

} // namespace neurender
