#include "Asset/MetaFile.h"
#include <chrono>
#include <fstream>


namespace neurender {

nlohmann::json MetaFile::ToJson() const {
  nlohmann::json j;
  j["guid"] = guid.ToString();
  j["originalPath"] = originalPath;
  j["assetType"] = assetType;
  j["lastModified"] = lastModified;
  return j;
}

MetaFile MetaFile::FromJson(const nlohmann::json &j) {
  MetaFile meta;
  meta.guid = UUID(j.value("guid", "0"));
  meta.originalPath = j.value("originalPath", "");
  meta.assetType = j.value("assetType", "unknown");
  meta.lastModified = j.value("lastModified", 0LL);
  return meta;
}

bool MetaFile::Save(const std::string &metaPath) const {
  std::ofstream file(metaPath);
  if (!file.is_open()) {
    return false;
  }
  file << ToJson().dump(2);
  return true;
}

MetaFile MetaFile::Load(const std::string &metaPath) {
  std::ifstream file(metaPath);
  if (!file.is_open()) {
    return MetaFile{};
  }

  try {
    nlohmann::json j;
    file >> j;
    return FromJson(j);
  } catch (...) {
    return MetaFile{};
  }
}

std::string MetaFile::GetMetaPath(const std::string &assetPath) {
  return assetPath + ".meta";
}

} // namespace neurender
