#pragma once

#include "Core/UUID.h"
#include "neugui_export.h"
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>

namespace neurender {

class Node;
class Scene;

/**
 * @brief EditorGUI 编辑器GUI管理器
 * 负责管理整个编辑器界面的布局和渲染
 */
class NEUGUI_API EditorGUI {
public:
  static void Initialize();
  static void Render();
  static void Shutdown();

  // 设置当前场景
  static void SetCurrentScene(std::shared_ptr<Scene> scene);
  static std::shared_ptr<Scene> GetCurrentScene() { return s_CurrentScene; }

  // 获取选中的节点
  static Node *GetSelectedNode() { return s_SelectedNode; }

private:
  // 主要面板渲染
  static void SetupDockSpace();
  static void RenderMenuBar();
  static void RenderSceneHierarchy();
  static void RenderInspector();
  static void RenderContentBrowser();

  // 辅助函数
  static void RenderNodeTree(Node *node);
  static void RenderTransformEditor(Node *node);
  static void RenderMeshNodeInspector(Node *node);
  static void RenderLightNodeInspector(Node *node);
  static void RenderCameraNodeInspector(Node *node);

  // 菜单功能
  static void MenuFile();
  static void MenuCreate();
  static void MenuDelete();
  static void MenuDebug();

  // 创建节点和场景
  static void CreateScene();
  static void CreateEmptyNode();
  static void CreateCube();
  static void CreateSphere();
  static void CreatePlane();
  static void CreatePointLight();
  static void CreateDirectionalLight();
  static void CreateCamera();
  static void DeleteSelectedNode();

  // 工程管理
  static void CreateNewProject();
  static void LoadProject();
  static void SaveProject();
  static std::string ShowFileDialog(bool isOpen, const char *filter);
  static void CreateFolder();

  // 纵向Tab实现
  static bool VerticalTab(const char *label, bool selected, const ImVec2 &size);

  // 状态
  static std::vector<std::shared_ptr<Scene>> s_Scenes; // 工程中的所有场景
  static int s_ActiveSceneIndex;                       // 当前激活的场景索引
  static std::shared_ptr<Scene> s_CurrentScene; // 当前编辑的场景 (为兼容性保留)
  static Node *s_SelectedNode;
  static int s_CurrentInspectorTab; // 当前选中的Inspector Tab

  // 文件浏览器状态
  static std::string s_CurrentPath;
  static std::string s_SelectedFile;
  static bool s_ShowNewFolderDialog;
  static char s_NewFolderName[256];

  // 渲染模式
  enum class RenderMode { Shaded, Wireframe, Albedo, Normal, Depth };
  static RenderMode s_RenderMode;

  // 布局初始化标志
  static bool s_DockSpaceInitialized;
};

// 简单的neuGUI类保持向后兼容
class NEUGUI_API neuGUI {
public:
  static void Render() { EditorGUI::Render(); }
};

} // namespace neurender
