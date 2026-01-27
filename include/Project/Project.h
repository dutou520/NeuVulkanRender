#pragma once
#include "Core/Object.h"
#include <json.hpp>
#include <memory>
#include <string>
#include <vector>


namespace neurender {

class Scene;

/**
 * @brief Project 类
 * 管理工程配置，包含 project.json 的读写
 */
class Project : public Object {
public:
  Project();
  explicit Project(const std::string &name);
  ~Project() = default;

  // 工程操作
  static std::shared_ptr<Project> Create(const std::string &projectPath,
                                         const std::string &name);
  static std::shared_ptr<Project> Load(const std::string &projectPath);
  bool Save() const;

  // 场景管理
  void AddScene(const std::string &scenePath);
  void RemoveScene(const std::string &scenePath);
  const std::vector<std::string> &GetScenePaths() const { return m_ScenePaths; }

  // Getters
  const std::string &GetProjectPath() const { return m_ProjectPath; }
  const std::string &GetAssetsPath() const { return m_AssetsPath; }
  const std::string &GetActiveScenePath() const { return m_ActiveScenePath; }

  // Setters
  void SetActiveScenePath(const std::string &path) { m_ActiveScenePath = path; }

  // JSON 序列化
  nlohmann::json ToJson() const;
  static std::shared_ptr<Project> FromJson(const nlohmann::json &j,
                                           const std::string &projectPath);

private:
  std::string m_ProjectPath;             // 工程根目录
  std::string m_AssetsPath;              // Assets 目录路径
  std::string m_ActiveScenePath;         // 当前激活的场景路径
  std::vector<std::string> m_ScenePaths; // 所有场景文件路径
};

} // namespace neurender
