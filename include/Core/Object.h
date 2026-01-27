#pragma once
#include "Core/UUID.h"
#include <string>

namespace neurender {

/**
 * @brief Object 基类
 * 所有节点和资源的基类，包含 UUID 和 Name
 */
class Object {
public:
  Object();
  explicit Object(const std::string &name);
  virtual ~Object() = default;

  // 禁止拷贝
  Object(const Object &) = delete;
  Object &operator=(const Object &) = delete;

  // 允许移动
  Object(Object &&) = default;
  Object &operator=(Object &&) = default;

  // Getters
  const UUID &GetUUID() const { return m_UUID; }
  const std::string &GetName() const { return m_Name; }

  // Setters
  void SetName(const std::string &name) { m_Name = name; }

protected:
  UUID m_UUID;
  std::string m_Name;
};

} // namespace neurender
