#include "Asset/AssetManager.h"
#include "neuLog.h"
#include <chrono>

namespace neurender {

AssetManager &AssetManager::GetInstance() {
  static AssetManager instance;
  return instance;
}

void AssetManager::Initialize(const std::string &assetsPath) {
  m_AssetsPath = assetsPath;

  // 确保 Assets 目录存在
  if (!std::filesystem::exists(m_AssetsPath)) {
    std::filesystem::create_directories(m_AssetsPath);
    LOG_I("Created Assets directory: {}", m_AssetsPath);
  }

  // 扫描现有资源
  ScanAssets();
}

void AssetManager::ScanAssets() {
  m_GUIDToPath.clear();
  m_PathToGUID.clear();

  if (m_AssetsPath.empty() || !std::filesystem::exists(m_AssetsPath)) {
    LOG_W("Assets path not set or does not exist");
    return;
  }

  // 递归扫描所有 .meta 文件
  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(m_AssetsPath)) {
    if (entry.is_regular_file() && entry.path().extension() == ".meta") {
      ProcessMetaFile(entry.path());
    }
  }

  LOG_I("Scanned {} assets from {}", m_GUIDToPath.size(), m_AssetsPath);
}

void AssetManager::ProcessMetaFile(const std::filesystem::path &metaPath) {
  MetaFile meta = MetaFile::Load(metaPath.string());

  if (!meta.guid.IsValid()) {
    LOG_W("Invalid meta file: {}", metaPath.string());
    return;
  }

  // 获取资源文件路径 (去掉 .meta 后缀)
  std::string assetPath = metaPath.string();
  assetPath = assetPath.substr(0, assetPath.length() - 5); // 去掉 ".meta"

  // 检查资源文件是否存在
  if (!std::filesystem::exists(assetPath)) {
    LOG_W("Asset file not found for meta: {}", assetPath);
    return;
  }

  // 注册映射
  m_GUIDToPath[meta.guid] = assetPath;
  m_PathToGUID[assetPath] = meta.guid;
}

std::string AssetManager::GetAssetPath(const UUID &guid) const {
  auto it = m_GUIDToPath.find(guid);
  if (it != m_GUIDToPath.end()) {
    return it->second;
  }
  return "";
}

UUID AssetManager::GetAssetGUID(const std::string &path) const {
  auto it = m_PathToGUID.find(path);
  if (it != m_PathToGUID.end()) {
    return it->second;
  }
  return UUID::Invalid();
}

UUID AssetManager::RegisterAsset(const std::string &assetPath,
                                 const std::string &assetType) {
  // 检查是否已注册
  auto existingGuid = GetAssetGUID(assetPath);
  if (existingGuid.IsValid()) {
    return existingGuid;
  }

  // 创建新的元数据
  MetaFile meta;
  meta.guid = UUID::Generate();
  meta.originalPath = assetPath;
  meta.assetType = assetType;

  // 获取当前时间戳
  auto now = std::chrono::system_clock::now();
  meta.lastModified =
      std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
          .count();

  // 保存 .meta 文件
  std::string metaPath = MetaFile::GetMetaPath(assetPath);
  if (!meta.Save(metaPath)) {
    LOG_E("Failed to save meta file: {}", metaPath);
    return UUID::Invalid();
  }

  // 注册映射
  m_GUIDToPath[meta.guid] = assetPath;
  m_PathToGUID[assetPath] = meta.guid;

  LOG_I("Registered asset: {} with GUID: {}", assetPath, meta.guid.ToString());
  return meta.guid;
}

bool AssetManager::HasAsset(const UUID &guid) const {
  return m_GUIDToPath.find(guid) != m_GUIDToPath.end();
}

bool AssetManager::HasAsset(const std::string &path) const {
  return m_PathToGUID.find(path) != m_PathToGUID.end();
}

} // namespace neurender
