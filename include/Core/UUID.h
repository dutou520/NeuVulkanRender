#pragma once
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>


namespace neurender {

/**
 * @brief UUID (Universally Unique Identifier) 类
 * 用于唯一标识资源和节点
 */
class UUID {
public:
  UUID();
  explicit UUID(uint64_t uuid);
  explicit UUID(const std::string &uuidString);

  operator uint64_t() const { return m_UUID; }

  bool operator==(const UUID &other) const { return m_UUID == other.m_UUID; }
  bool operator!=(const UUID &other) const { return m_UUID != other.m_UUID; }
  bool operator<(const UUID &other) const { return m_UUID < other.m_UUID; }

  std::string ToString() const;
  bool IsValid() const { return m_UUID != 0; }

  static UUID Generate();
  static UUID Invalid() { return UUID(static_cast<uint64_t>(0)); }

private:
  uint64_t m_UUID;
};

} // namespace neurender

// 为 std::hash 特化, 允许 UUID 作为 unordered_map 的 key
namespace std {
template <> struct hash<neurender::UUID> {
  size_t operator()(const neurender::UUID &uuid) const {
    return hash<uint64_t>()(static_cast<uint64_t>(uuid));
  }
};
} // namespace std
