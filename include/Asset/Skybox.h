#pragma once

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace neurender {

/**
 * @brief 天空盒记录结构体
 * 包含6张面贴图的路径、天空盒旋转角度、亮度
 */
struct SkyboxSettings {
  std::string name;
  std::string filePath;               // .skybox 文件路径
  std::vector<std::string> facePaths; // top, right, left, front, back, bottom
  float rotationY = 0.0f;
  float brightness = 1.0f;

  bool LoadFromFile(const std::string &path) {
    std::ifstream ifs(std::filesystem::u8path(path));
    if (!ifs)
      return false;

    try {
      nlohmann::json j;
      ifs >> j;
      name = j.value("name", "Unnamed Skybox");
      facePaths = j.value("facePaths", std::vector<std::string>(6, ""));
      if (facePaths.size() != 6) {
        facePaths.resize(6, "");
      }
      rotationY = j.value("rotationY", 0.0f);
      brightness = j.value("brightness", 1.0f);
      filePath = path;
      return true;
    } catch (const std::exception &e) {
      return false;
    }
  }

  bool SaveToFile(const std::string &path) const {
    nlohmann::json j;
    j["name"] = name;
    j["facePaths"] = facePaths;
    j["rotationY"] = rotationY;
    j["brightness"] = brightness;

    std::ofstream ofs(std::filesystem::u8path(path));
    if (!ofs)
      return false;
    ofs << j.dump(4);
    return true;
  }
};

} // namespace neurender
