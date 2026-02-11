#pragma once
#include <array>
#include <glm/glm.hpp>
#include <vector>

namespace neurender {

struct AABB {
  glm::vec3 min = glm::vec3(1e30f);
  glm::vec3 max = glm::vec3(-1e30f);

  void Merge(const glm::vec3 &p) {
    min = glm::min(min, p);
    max = glm::max(max, p);
  }

  void Merge(const AABB &other) {
    min = glm::min(min, other.min);
    max = glm::max(max, other.max);
  }

  glm::vec3 Center() const { return (min + max) * 0.5f; }
  glm::vec3 Extents() const { return (max - min) * 0.5f; }

  AABB Transform(const glm::mat4 &m) const {
    glm::vec3 corners[8] = {{min.x, min.y, min.z}, {max.x, min.y, min.z},
                            {min.x, max.y, min.z}, {max.x, max.y, min.z},
                            {min.x, min.y, max.z}, {max.x, min.y, max.z},
                            {min.x, max.y, max.z}, {max.x, max.y, max.z}};
    AABB res;
    for (int i = 0; i < 8; i++) {
      res.Merge(glm::vec3(m * glm::vec4(corners[i], 1.0f)));
    }
    return res;
  }
};

struct Frustum {
  std::array<glm::vec4, 6> planes; // ax + by + cz + d = 0

  void FromViewProj(const glm::mat4 &m) {
    // Gribb-Hartmann method
    planes[0] = glm::vec4(m[0][3] + m[0][0], m[1][3] + m[1][0],
                          m[2][3] + m[2][0], m[3][3] + m[3][0]); // Left
    planes[1] = glm::vec4(m[0][3] - m[0][0], m[1][3] - m[1][0],
                          m[2][3] - m[2][0], m[3][3] - m[3][0]); // Right
    planes[2] = glm::vec4(m[0][3] + m[0][1], m[1][3] + m[1][1],
                          m[2][3] + m[2][1], m[3][3] + m[3][1]); // Bottom
    planes[3] = glm::vec4(m[0][3] - m[0][1], m[1][3] - m[1][1],
                          m[2][3] - m[2][1], m[3][3] - m[3][1]); // Top
    planes[4] = glm::vec4(m[0][3] + m[0][2], m[1][3] + m[1][2],
                          m[2][3] + m[2][2], m[3][3] + m[3][2]); // Near
    planes[5] = glm::vec4(m[0][3] - m[0][2], m[1][3] - m[1][2],
                          m[2][3] - m[2][2], m[3][3] - m[3][2]); // Far

    // Normalize
    for (auto &plane : planes) {
      float length = glm::length(glm::vec3(plane));
      plane /= length;
    }
  }

  bool TestAABB(const AABB &aabb) const {
    for (int i = 0; i < 6; i++) {
      glm::vec3 positive = aabb.min;
      if (planes[i].x >= 0)
        positive.x = aabb.max.x;
      if (planes[i].y >= 0)
        positive.y = aabb.max.y;
      if (planes[i].z >= 0)
        positive.z = aabb.max.z;

      if (glm::dot(glm::vec3(planes[i]), positive) + planes[i].w < 0) {
        return false;
      }
    }
    return true;
  }

  // 计算包围球 (用于阴影相机)
  void GetBoundingSphere(const glm::mat4 &invViewProj, glm::vec3 &center,
                         float &radius) const {
    glm::vec3 corners[8];
    glm::vec4 clipCorners[8] = {{-1, -1, 0, 1}, {1, -1, 0, 1},  {-1, 1, 0, 1},
                                {1, 1, 0, 1},   {-1, -1, 1, 1}, {1, -1, 1, 1},
                                {-1, 1, 1, 1},  {1, 1, 1, 1}};

    center = glm::vec3(0, 0, 0);
    for (int i = 0; i < 8; i++) {
      glm::vec4 worldPos = invViewProj * clipCorners[i];
      corners[i] = glm::vec3(worldPos) / worldPos.w;
      center += corners[i];
    }
    center /= 8.0f;

    radius = 0;
    for (int i = 0; i < 8; i++) {
      radius = glm::max(radius, glm::distance(center, corners[i]));
    }
  }
};

} // namespace neurender
