#include "Asset/AssetManager.h"
#include "Asset/MetaFile.h"
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
    LOG_W("Assets path not set or does not exist: {}", m_AssetsPath);
    return;
  }

  // 递归扫描所有 .meta 文件
  uint32_t metaCount = 0;
  try {
    for (const auto &entry :
         std::filesystem::recursive_directory_iterator(m_AssetsPath)) {
      try {
        if (entry.is_regular_file() && entry.path().extension() == ".meta") {
          ProcessMetaFile(entry.path());
          metaCount++;
        }
      } catch (const std::exception &e) {
        LOG_W("Error processing entry in ScanAssets: {}", e.what());
      }
    }
  } catch (const std::exception &e) {
    LOG_E("Recursive directory iterator failed in ScanAssets: {}", e.what());
  }

  LOG_I("Finished scanning assets. Found {} .meta files, registered {} valid "
        "assets from {}",
        metaCount, m_GUIDToPath.size(), m_AssetsPath);
}

void AssetManager::ProcessMetaFile(const std::filesystem::path &metaPath) {
  try {
    MetaFile meta = MetaFile::Load(metaPath);

    if (!meta.guid.IsValid()) {
      LOG_W("Invalid meta file: {}", metaPath.string());
      return;
    }

    // 获取资源文件路径 (去掉 .meta 后缀)
    std::filesystem::path assetPath = metaPath;
    assetPath.replace_extension("");

    // 检查资源文件是否存在
    if (!std::filesystem::exists(assetPath)) {
      LOG_W("Asset file not found for meta: {}. Tried path: {}",
            metaPath.string(), assetPath.string());
      return;
    }

    // 注册映射
    m_GUIDToPath[meta.guid] = assetPath;
    m_PathToGUID[assetPath.generic_string()] = meta.guid;

    LOG_I("Registered asset: {} -> {}", meta.guid.ToString(),
          assetPath.generic_string());
  } catch (const std::exception &e) {
    LOG_E("Failed to process meta file {}: {}", metaPath.string(), e.what());
  }
}

std::string AssetManager::GetAssetPath(const UUID &guid) const {
  auto it = m_GUIDToPath.find(guid);
  if (it != m_GUIDToPath.end()) {
    // Return potentially lossy string for compatibility, but the internal path
    // is safe
    return it->second.string();
  }
  return "";
}

std::filesystem::path AssetManager::GetAssetPathObj(const UUID &guid) const {
  auto it = m_GUIDToPath.find(guid);
  if (it != m_GUIDToPath.end()) {
    return it->second;
  }
  return std::filesystem::path();
}

UUID AssetManager::GetAssetGUID(const std::string &path) const {
  auto it = m_PathToGUID.find(path);
  if (it != m_PathToGUID.end()) {
    return it->second;
  }
  return UUID::Invalid();
}

UUID AssetManager::GetAssetGUID(const std::filesystem::path &path) const {
  return GetAssetGUID(path.generic_string());
}

UUID AssetManager::RegisterAsset(const std::string &assetPath,
                                 const std::string &assetType) {
  return RegisterAsset(std::filesystem::u8path(assetPath), assetType);
}

UUID AssetManager::RegisterAsset(const std::filesystem::path &assetPath,
                                 const std::string &assetType) {
  // 检查是否已注册
  auto existingGuid = GetAssetGUID(assetPath);
  if (existingGuid.IsValid()) {
    return existingGuid;
  }

  // 创建新的元数据
  MetaFile meta;
  meta.guid = UUID::Generate();
  meta.originalPath = assetPath.generic_string();
  meta.assetType = assetType;

  // 获取当前时间戳
  auto now = std::chrono::system_clock::now();
  meta.lastModified =
      std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
          .count();

  // 保存 .meta 文件
  std::filesystem::path metaPath = MetaFile::GetMetaPath(assetPath);
  if (!meta.Save(metaPath)) {
    LOG_E("Failed to save meta file: {}", metaPath.string());
    return UUID::Invalid();
  }

  // 注册映射
  m_GUIDToPath[meta.guid] = assetPath;
  m_PathToGUID[assetPath.generic_string()] = meta.guid;

  LOG_I("Registered asset: {} with GUID: {}", assetPath.generic_string(),
        meta.guid.ToString());
  return meta.guid;
}

bool AssetManager::HasAsset(const UUID &guid) const {
  return m_GUIDToPath.find(guid) != m_GUIDToPath.end();
}

bool AssetManager::HasAsset(const std::string &path) const {
  return m_PathToGUID.find(path) != m_PathToGUID.end();
}

bool AssetManager::HasAsset(const std::filesystem::path &path) const {
  return HasAsset(path.generic_string());
}

void AssetManager::UnregisterAsset(const UUID &guid) {
  auto it = m_GUIDToPath.find(guid);
  if (it != m_GUIDToPath.end()) {
    std::string pathStr = it->second.generic_string();
    m_PathToGUID.erase(pathStr);
    m_GUIDToPath.erase(it);
    LOG_I("Unregistered asset: {}", guid.ToString());
  }
}

void AssetManager::UnregisterAsset(const std::filesystem::path &assetPath) {
  std::string pathStr = assetPath.generic_string();
  auto it = m_PathToGUID.find(pathStr);
  if (it != m_PathToGUID.end()) {
    m_GUIDToPath.erase(it->second);
    m_PathToGUID.erase(it);
    LOG_I("Unregistered asset: {}", pathStr);
  }
}

} // namespace neurender
