#pragma once
#include "Core/UUID.h"
#include <filesystem>
#include <string>
#include <unordered_map>

namespace neurender {

/**
 * @brief AssetManager 单例类
 * 管理 GUID -> 文件路径 的映射，扫描 Assets 目录
 */
class AssetManager {
public:
  // 获取单例
  static AssetManager &GetInstance();

  // 禁止拷贝和移动
  AssetManager(const AssetManager &) = delete;
  AssetManager &operator=(const AssetManager &) = delete;
  AssetManager(AssetManager &&) = delete;
  AssetManager &operator=(AssetManager &&) = delete;

  // 初始化，设置 Assets 目录路径
  void Initialize(const std::string &assetsPath);

  // 扫描 Assets 目录，建立 GUID -> 路径映射
  void ScanAssets();

  // 根据 GUID 获取文件路径
  std::string GetAssetPath(const UUID &guid) const;
  std::filesystem::path GetAssetPathObj(const UUID &guid) const;

  // 根据路径获取 GUID
  UUID GetAssetGUID(const std::string &path) const;
  UUID GetAssetGUID(const std::filesystem::path &path) const;

  // 注册新资源（创建 .meta 文件）
  UUID RegisterAsset(const std::string &assetPath,
                     const std::string &assetType);
  UUID RegisterAsset(const std::filesystem::path &assetPath,
                     const std::string &assetType);

  // 检查资源是否存在
  bool HasAsset(const UUID &guid) const;
  bool HasAsset(const std::string &path) const;
  bool HasAsset(const std::filesystem::path &path) const;

  // 注销资源（不删除文件，只清除映射）
  void UnregisterAsset(const UUID &guid);
  void UnregisterAsset(const std::filesystem::path &assetPath);

  // 获取 Assets 目录路径
  const std::string &GetAssetsPath() const { return m_AssetsPath; }

  // 获取所有已注册资源
  const std::unordered_map<UUID, std::filesystem::path> &GetAllAssets() const {
    return m_GUIDToPath;
  }

private:
  AssetManager() = default;
  ~AssetManager() = default;

  // 处理单个 meta 文件
  void ProcessMetaFile(const std::filesystem::path &metaPath);

  std::string m_AssetsPath;
  std::unordered_map<UUID, std::filesystem::path> m_GUIDToPath;
  std::unordered_map<std::string, UUID> m_PathToGUID;
};

} // namespace neurender
