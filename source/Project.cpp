#include "Project/Project.h"
#include "Asset/AssetManager.h"
#include "neuLog.h"
#include <filesystem>
#include <fstream>

/**
 * @file Project.cpp
 * @brief 项目管理类的实现，负责项目的创建、加载、保存以及场景管理
 */

namespace neurender {

// 默认构造函数，创建一个名为 "New Project" 的项目
Project::Project() : Object("New Project") {}

// 构造函数，创建一个指定名称的项目
Project::Project(const std::string &name) : Object(name) {}

/**
 * @brief 创建一个新项目
 * @param projectPath 项目保存的根目录路径
 * @param name 项目名称
 * @return 返回创建的项目实例的智能指针，失败则返回 nullptr
 */
std::shared_ptr<Project> Project::Create(const std::string &projectPath,
                                         const std::string &name) {
  // 1. 创建项目实例
  auto project = std::make_shared<Project>(name);
  project->m_ProjectPath = projectPath;
  project->m_AssetsPath = projectPath + "/Assets";

  // 2. 创建物理目录结构
  // 创建项目根目录和资源子目录
  std::filesystem::create_directories(projectPath);
  std::filesystem::create_directories(project->m_AssetsPath);

  // 3. 将项目元数据保存到 project.json
  if (!project->Save()) {
    LOG_E("Failed to create project at: {}", projectPath);
    return nullptr;
  }

  // 4. 初始化资源管理器，设置基准资源路径
  AssetManager::GetInstance().Initialize(project->m_AssetsPath);

  LOG_I("Created project: {} at {}", name, projectPath);
  return project;
}

/**
 * @brief 从指定路径加载项目
 * @param projectPath 项目根目录路径
 * @return 返回加载的项目实例的智能指针，失败则返回 nullptr
 */
std::shared_ptr<Project> Project::Load(const std::string &projectPath) {
  std::filesystem::path projectPathObj(projectPath);
  std::filesystem::path projectFile = projectPathObj / "project.json";

  // 打开 project.json 文件
  std::ifstream file(projectFile);
  if (!file.is_open()) {
    LOG_E("Failed to open project file: {}", projectFile.string());
    return nullptr;
  }

  try {
    // 解析 JSON 数据
    nlohmann::json j;
    file >> j;

    // 调用 FromJson 将 JSON 数据转换回 Project 对象
    auto project = FromJson(j, projectPath);

    if (project) {
      // 加载成功后，初始化资源管理器
      AssetManager::GetInstance().Initialize(project->m_AssetsPath);
      LOG_I("Loaded project: {} from {}", project->GetName(), projectPath);
    }

    return project;
  } catch (const std::exception &e) {
    LOG_E("Failed to parse project file: {}", e.what());
    return nullptr;
  }
}

/**
 * @brief 保存项目配置到 project.json 文件
 * @return 保存成功返回 true，否则返回 false
 */
bool Project::Save() const {
  std::filesystem::path projectFile =
      std::filesystem::path(m_ProjectPath) / "project.json";

  std::ofstream file(projectFile);
  if (!file.is_open()) {
    LOG_E("Failed to save project file: {}", projectFile.string());
    return false;
  }

  file << ToJson().dump(2);
  return true;
}

/**
 * @brief 向项目中添加一个场景路径
 * @param scenePath 场景文件的相对路径或绝对路径
 */
void Project::AddScene(const std::string &scenePath) {
  // 检查场景路径是否已存在，避免重复添加
  for (const auto &path : m_ScenePaths) {
    if (path == scenePath) {
      return;
    }
  }
  m_ScenePaths.push_back(scenePath);
}

/**
 * @brief 从项目中移除一个场景路径
 * @param scenePath 要移除的场景路径
 */
void Project::RemoveScene(const std::string &scenePath) {
  // 使用 erase-remove 惯用法从列表中移除指定的场景路径
  m_ScenePaths.erase(
      std::remove(m_ScenePaths.begin(), m_ScenePaths.end(), scenePath),
      m_ScenePaths.end());
}

/**
 * @brief 将项目信息序列化为 JSON 对象
 * @return 返回包含项目信息的 JSON 对象
 */
nlohmann::json Project::ToJson() const {
  nlohmann::json j;
  j["uuid"] = m_UUID.ToString();        // 项目唯一标识符
  j["name"] = m_Name;                   // 项目名称
  j["assetsPath"] = "Assets";           // 资源根目录名（相对项目路径）
  j["activeScene"] = m_ActiveScenePath; // 当前激活的场景路径
  j["scenes"] = m_ScenePaths;           // 项目包含的所有场景列表
  return j;
}

/**
 * @brief 从 JSON 对象反序列化出一个项目实例
 * @param j 包含项目信息的 JSON 对象
 * @param projectPath 项目所在的根目录路径
 * @return 返回反序列化后的项目实例智能指针
 */
std::shared_ptr<Project> Project::FromJson(const nlohmann::json &j,
                                           const std::string &projectPath) {
  auto project = std::make_shared<Project>();
  project->m_ProjectPath = projectPath;
  // 读取项目名称，如果不存在则赋予默认值
  project->m_Name = j.value("name", "Unnamed Project");
  // 拼接完整的资源路径
  project->m_AssetsPath = projectPath + "/" + j.value("assetsPath", "Assets");
  // 获取当前激活场景
  project->m_ActiveScenePath = j.value("activeScene", "");

  // 读取场景列表
  if (j.contains("scenes") && j["scenes"].is_array()) {
    for (const auto &scenePath : j["scenes"]) {
      project->m_ScenePaths.push_back(scenePath.get<std::string>());
    }
  }

  return project;
}

} // namespace neurender
