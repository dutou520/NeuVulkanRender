#pragma once

#include "neugui_export.h"
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace neurender {

class Node;
class Scene;
class PointLightNode;

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

  // 从路径加载工程/场景（供命令行等外部系统调用）
  // projectFile 可以是工程目录或 project.json 文件路径
  static bool LoadProjectFromPath(const std::string &projectFile);
  static bool LoadSceneFromPath(const std::string &scenePath);

  // Scene View 和输入转发
  static void RenderSceneView();
  static void SetSceneViewResizeCallback(std::function<void(uint32_t, uint32_t)> callback) {
    s_SceneViewResizeCallback = std::move(callback);
  }
  static bool IsSceneViewHovered() { return s_SceneViewHovered; }
  static bool IsSceneViewFocused() { return s_SceneViewFocused; }
  static bool IsSceneViewRightClicked() { return s_SceneViewRightClicked; }
  // 获取 SceneView 视口中心（屏幕坐标），尺寸非法时返回 {-1,-1}
  static ImVec2 GetSceneViewCenter() {
    if (s_SceneViewSize.x <= 0.0f || s_SceneViewSize.y <= 0.0f) {
      return ImVec2(-1.0f, -1.0f);
    }
    return ImVec2(s_SceneViewPos.x + s_SceneViewSize.x * 0.5f,
                  s_SceneViewPos.y + s_SceneViewSize.y * 0.5f);
  }

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
  static void RenderPointLightInspector(PointLightNode *light);
  static void RenderCameraNodeInspector(Node *node);
  static void RenderPostProcessInspector();

  // 菜单功能
  static void MenuFile();
  static void MenuCreate();
  static void MenuDebug();
  static void MenuPerformance();

  // 创建节点和场景
  static void CreateScene();
  static void CreateEmptyNode();
  static void CreateCube();
  static void CreateSphere();
  static void CreatePlane();
  static void CreatePointLight();
  static void CreateCamera();
  static void DeleteSelectedNode();

  // 工程管理
  static void CreateNewProject();
  static void LoadProject();
  static void SaveProject();
  static std::string ShowFileDialog(bool isOpen, const char *filter);
  static void CreateFolder();

  // Clipboard operations
  static std::string GetClipboardPath() { return s_ClipboardPath; }
  static bool HasClipboard() { return !s_ClipboardPath.empty(); }

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
  static bool s_ShowRenameDialog;
  static char s_RenameBuffer[256];
  static std::string s_RenameTargetFile;

  // Clipboard for cut/paste operations
  static std::string s_ClipboardPath;

  // Pending node operations (to avoid modifying tree during traversal)
  static Node *s_PendingReparentSource;
  static Node *s_PendingReparentTarget;
  static Node *s_PendingDeleteNode;
  static Node *s_PendingCloneSource;
  static void ProcessPendingNodeOperations();

  // 渲染模式
  enum class RenderMode {
    Shaded,
    Wireframe,
    Albedo,
    Normal,
    Depth,
    Smoothness,
    Specular,
    Occlusion,
    MaterialFlags,
    ShadingID,
    Emission
  };
  static RenderMode s_RenderMode;
  static RenderMode GetRenderMode() { return s_RenderMode; }

  // 布局初始化标志
  static bool s_DockSpaceInitialized;
  // 布局脏标记：工程加载后下一帧重建 DockSpace（从 ini 设置恢复，或重置为默认）
  static bool s_DockSpaceLayoutDirty;

  // Scene View 状态
  static std::function<void(uint32_t, uint32_t)> s_SceneViewResizeCallback;
  static bool s_SceneViewHovered;
  static bool s_SceneViewFocused;
  static bool s_SceneViewRightClicked;
  static ImVec2 s_SceneViewSize;
  // SceneView 视口内容区在屏幕上的位置（用于鼠标环绕中心）
  static ImVec2 s_SceneViewPos;
};

// 简单的neuGUI类保持向后兼容
class NEUGUI_API neuGUI {
public:
  static void Render() { EditorGUI::Render(); }
};

} // namespace neurender
