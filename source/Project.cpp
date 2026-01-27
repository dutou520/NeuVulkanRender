#include "Project/Project.h"
#include "Asset/AssetManager.h"
#include "neuLog.h"
#include <filesystem>
#include <fstream>


namespace neurender {

Project::Project() : Object("New Project") {}

Project::Project(const std::string &name) : Object(name) {}

std::shared_ptr<Project> Project::Create(const std::string &projectPath,
                                         const std::string &name) {
  auto project = std::make_shared<Project>(name);
  project->m_ProjectPath = projectPath;
  project->m_AssetsPath = projectPath + "/Assets";

  // 创建项目目录结构
  std::filesystem::create_directories(projectPath);
  std::filesystem::create_directories(project->m_AssetsPath);

  // 保存 project.json
  if (!project->Save()) {
    LOG_E("Failed to create project at: {}", projectPath);
    return nullptr;
  }

  // 初始化 AssetManager
  AssetManager::GetInstance().Initialize(project->m_AssetsPath);

  LOG_I("Created project: {} at {}", name, projectPath);
  return project;
}

std::shared_ptr<Project> Project::Load(const std::string &projectPath) {
  std::string projectFile = projectPath + "/project.json";

  std::ifstream file(projectFile);
  if (!file.is_open()) {
    LOG_E("Failed to open project file: {}", projectFile);
    return nullptr;
  }

  try {
    nlohmann::json j;
    file >> j;
    auto project = FromJson(j, projectPath);

    if (project) {
      // 初始化 AssetManager
      AssetManager::GetInstance().Initialize(project->m_AssetsPath);
      LOG_I("Loaded project: {} from {}", project->GetName(), projectPath);
    }

    return project;
  } catch (const std::exception &e) {
    LOG_E("Failed to parse project file: {}", e.what());
    return nullptr;
  }
}

bool Project::Save() const {
  std::string projectFile = m_ProjectPath + "/project.json";

  std::ofstream file(projectFile);
  if (!file.is_open()) {
    LOG_E("Failed to save project file: {}", projectFile);
    return false;
  }

  file << ToJson().dump(2);
  return true;
}

void Project::AddScene(const std::string &scenePath) {
  // 检查是否已存在
  for (const auto &path : m_ScenePaths) {
    if (path == scenePath) {
      return;
    }
  }
  m_ScenePaths.push_back(scenePath);
}

void Project::RemoveScene(const std::string &scenePath) {
  m_ScenePaths.erase(
      std::remove(m_ScenePaths.begin(), m_ScenePaths.end(), scenePath),
      m_ScenePaths.end());
}

nlohmann::json Project::ToJson() const {
  nlohmann::json j;
  j["uuid"] = m_UUID.ToString();
  j["name"] = m_Name;
  j["assetsPath"] = "Assets"; // 相对路径
  j["activeScene"] = m_ActiveScenePath;
  j["scenes"] = m_ScenePaths;
  return j;
}

std::shared_ptr<Project> Project::FromJson(const nlohmann::json &j,
                                           const std::string &projectPath) {
  auto project = std::make_shared<Project>();
  project->m_ProjectPath = projectPath;
  project->m_Name = j.value("name", "Unnamed Project");
  project->m_AssetsPath = projectPath + "/" + j.value("assetsPath", "Assets");
  project->m_ActiveScenePath = j.value("activeScene", "");

  if (j.contains("scenes") && j["scenes"].is_array()) {
    for (const auto &scenePath : j["scenes"]) {
      project->m_ScenePaths.push_back(scenePath.get<std::string>());
    }
  }

  return project;
}

} // namespace neurender
