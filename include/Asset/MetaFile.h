#pragma once
#include "Core/UUID.h"
#include <json.hpp>
#include <string>


namespace neurender {

/**
 * @brief MetaFile 结构
 * 用于存储资源的元数据信息
 */
struct MetaFile {
  UUID guid;                // 资源的唯一标识符
  std::string originalPath; // 原始文件路径
  std::string assetType;    // 资源类型: "mesh", "texture", "material", "audio"
  int64_t lastModified;     // 最后修改时间戳

  // JSON 序列化
  nlohmann::json ToJson() const;
  static MetaFile FromJson(const nlohmann::json &j);

  // 文件操作
  bool Save(const std::string &metaPath) const;
  static MetaFile Load(const std::string &metaPath);

  // 生成 meta 文件路径
  static std::string GetMetaPath(const std::string &assetPath);
};

} // namespace neurender
