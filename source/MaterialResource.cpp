#include "Asset/MaterialResource.h"
#include "neuLog.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace neurender {

bool MaterialResource::LoadFromFile(const std::string &path) {
  try {
    std::ifstream file(path);
    if (!file.is_open()) {
      LOG_E("Failed to open material file: {}", path);
      return false;
    }

    nlohmann::json j;
    file >> j;
    file.close();

    material = Material::FromJson(j);
    filePath = path;
    name = material.name;

    LOG_I("Loaded material: {} from {}", name, path);
    isLoaded = true;
    return true;

  } catch (const std::exception &e) {
    LOG_E("Failed to parse material file {}: {}", path, e.what());
    return false;
  }
}

bool MaterialResource::SaveToFile(const std::string &path) const {
  try {
    nlohmann::json j = material.ToJson();

    std::filesystem::path fullPath = std::filesystem::u8path(path);
    std::filesystem::create_directories(fullPath.parent_path());

    std::ofstream file(fullPath);
    if (!file.is_open()) {
      LOG_E("Failed to create material file: {}", path);
      return false;
    }

    file << j.dump(2);
    file.close();

    LOG_I("Saved material: {} to {}", material.name, path);
    return true;

  } catch (const std::exception &e) {
    LOG_E("Failed to save material file {}: {}", path, e.what());
    return false;
  }
}

bool MaterialResource::HasTextures() const {
  return baseColorTex != nullptr || metallicTex != nullptr ||
         roughnessTex != nullptr || normalTex != nullptr ||
         emissiveTex != nullptr || occlusionTex != nullptr;
}

uint32_t MaterialResource::GetTextureFlags() const {
  uint32_t flags = 0;
  if (baseColorTex != nullptr && baseColorTex->isLoaded)
    flags |= (1 << 0);
  if (metallicTex != nullptr && metallicTex->isLoaded)
    flags |= (1 << 1);
  if (normalTex != nullptr && normalTex->isLoaded)
    flags |= (1 << 2);
  if (emissiveTex != nullptr && emissiveTex->isLoaded)
    flags |= (1 << 3);
  if (occlusionTex != nullptr && occlusionTex->isLoaded)
    flags |= (1 << 4);
  if (roughnessTex != nullptr && roughnessTex->isLoaded)
    flags |= (1 << 5);
  return flags;
}

} // namespace neurender
