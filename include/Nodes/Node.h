#pragma once
#include "neuvulkanrenderlib_export.h"

#include "glm/ext/vector_float3.hpp"
#include "glm/glm.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
namespace neurender {
class NEUVULKANRENDERLIB_API Node {
public:
  Node();
  Node(const Node &) = delete;
  Node &operator=(const Node &) = delete;
  bool enabled;
  void SetPos(glm::vec3);
  void SetEulerAngle(glm::vec3);
  void SetScale(glm::vec3);

  glm::vec3 position;
  glm::vec3 euler_angle;
  glm::vec3 scale;
  glm::vec4 rotation;
};
} // namespace neurender
