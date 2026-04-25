#include "neuGUI.h"
#include "Asset/AssetManager.h"
#include "Asset/ModelImporter.h"
#include "Nodes/CameraNode.h"
#include "Nodes/MeshNode.h"
#include "Nodes/Node.h"
#include "Nodes/PointLightNode.h"
#include "Project/Project.h"
#include "RenderCore.h"
#include "Scene/Scene.h"
#include "Window.h"
#include "neuLog.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <imgui_internal.h>
#include <nlohmann/json.hpp>
#include <set>
#include <shlobj.h>
#include <windows.h>

#define min(a, b) (((a) < (b)) ? (a) : (b))
namespace neurender {

static void ClearMaterialInScene(Node *node, const UUID &matID) {
  if (!node)
    return;
  if (node->GetNodeType() == "MeshNode") {
    auto *meshNode = static_cast<MeshNode *>(node);
    if (meshNode->GetMaterialID() == matID) {
      meshNode->SetMaterialID(UUID::Invalid());
    }
  }
  for (auto &child : node->GetChildren()) {
    ClearMaterialInScene(child.get(), matID);
  }
}

// 静态成员初始化
std::vector<std::shared_ptr<Scene>> EditorGUI::s_Scenes;
int EditorGUI::s_ActiveSceneIndex = -1;
std::shared_ptr<Scene> EditorGUI::s_CurrentScene = nullptr;
Node *EditorGUI::s_SelectedNode = nullptr;
int EditorGUI::s_CurrentInspectorTab = 0;
std::string EditorGUI::s_CurrentPath = "";
std::string EditorGUI::s_SelectedFile = "";
bool EditorGUI::s_ShowNewFolderDialog = false;
char EditorGUI::s_NewFolderName[256] = "";
bool EditorGUI::s_ShowRenameDialog = false;
char EditorGUI::s_RenameBuffer[256] = "";
std::string EditorGUI::s_RenameTargetFile = "";
Node *EditorGUI::s_PendingReparentSource = nullptr;
Node *EditorGUI::s_PendingReparentTarget = nullptr;
Node *EditorGUI::s_PendingDeleteNode = nullptr;
Node *EditorGUI::s_PendingCloneSource = nullptr;
EditorGUI::RenderMode EditorGUI::s_RenderMode = RenderMode::Shaded;
bool EditorGUI::s_DockSpaceInitialized = false;
std::string EditorGUI::s_ClipboardPath = "";

static void RenderTextureSlot(const char *label, const UUID &materialID,
                              uint32_t binding, TextureResource *texRes,
                              const UUID &texID) {
  ImGui::BeginGroup();
  ImGui::Text("%s", label);

  ImTextureID imTexID = RenderCore::GetImGuiTextureID(texID);

  ImVec2 size(64, 64);
  ImVec4 bg_col = ImVec4(0, 0, 0, 1);
  ImVec4 tint_col = ImVec4(1, 1, 1, 1);

  std::string idStr = std::string("##Slot") + std::to_string(binding);

  // ImGui 1.89+ ImageButton signature
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
  if (ImGui::ImageButton(idStr.c_str(), imTexID, size, ImVec2(0, 0),
                         ImVec2(1, 1), bg_col, tint_col)) {
  }
  ImGui::PopStyleVar();

  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload *payload =
            ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
      const char *path = (const char *)payload->Data;
      UUID newTexID =
          AssetManager::GetInstance().GetAssetGUID(std::filesystem::path(path));
      if (newTexID.IsValid()) {
        RenderCore::SetMaterialTexture(materialID, binding, newTexID);
      }
    }
    ImGui::EndDragDropTarget();
  }

  ImGui::SameLine();
  ImGui::BeginGroup();
  std::string texName = "None/Default";
  if (texRes && texID.IsValid()) {
    texName = std::filesystem::path(texRes->filePath).filename().u8string();
  }
  ImGui::TextWrapped("File: %s", texName.c_str());

  if (texID.IsValid()) {
    if (ImGui::Button(
            (std::string("Remove##") + std::to_string(binding)).c_str())) {
      RenderCore::SetMaterialTexture(materialID, binding, UUID::Invalid());
    }
  }
  ImGui::EndGroup();
  ImGui::EndGroup();
  ImGui::Spacing();
}

// CubeMap 面拖放预览槽位
static void RenderCubeFaceSlot(const char *label, std::string &facePath,
                               int faceIndex) {
  ImGui::PushID(faceIndex);
  ImGui::BeginGroup();

  // 标签
  ImGui::TextDisabled("%s", label);

  // 获取预览图
  ImTextureID thumb = facePath.empty()
                          ? (ImTextureID)0
                          : RenderCore::GetImGuiTextureIDByPath(facePath);

  ImVec2 btnSize(64.0f, 64.0f);
  ImVec4 bgCol = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
  ImVec4 tintCol = ImVec4(1, 1, 1, 1);

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
  ImGui::ImageButton("##face", thumb, btnSize, ImVec2(0, 0), ImVec2(1, 1),
                     bgCol, tintCol);
  ImGui::PopStyleVar();

  // 拖放目标
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload *payload =
            ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
      const char *droppedPath = (const char *)payload->Data;
      std::string ext =
          std::filesystem::path(droppedPath).extension().u8string();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
      // 支持常见图片格式
      if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" ||
          ext == ".hdr" || ext == ".exr" || ext == ".tga") {
        facePath = droppedPath;
      }
    }
    ImGui::EndDragDropTarget();
  }

  // 显示文件名（截断）
  ImGui::SameLine();
  ImGui::BeginGroup();
  if (facePath.empty()) {
    ImGui::TextDisabled("None");
  } else {
    std::string fname = std::filesystem::path(facePath).filename().u8string();
    if (fname.size() > 20)
      fname = fname.substr(0, 17) + "...";
    ImGui::TextWrapped("%s", fname.c_str());
    if (ImGui::SmallButton("X")) {
      facePath.clear();
    }
  }
  ImGui::SameLine();
  // 输入设置小按鈕
  if (ImGui::SmallButton("...")) {
    ImGui::OpenPopup("##FacePathInput");
  }
  if (ImGui::BeginPopup("##FacePathInput")) {
    static char bufs[6][512] = {};
    if (ImGui::IsWindowAppearing()) {
      strncpy(bufs[faceIndex], facePath.c_str(), sizeof(bufs[faceIndex]));
    }
    if (ImGui::InputText("Path", bufs[faceIndex], sizeof(bufs[faceIndex]),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      facePath = bufs[faceIndex];
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  ImGui::EndGroup();

  ImGui::EndGroup();
  ImGui::Spacing();
  ImGui::PopID();
}

void EditorGUI::Initialize() {
  ImGui::GetIO().FontGlobalScale = 1.5f; // 全局 UI 字体放大 1.2-1.5 倍
  LOG_I("EditorGUI Initialized");
}

void EditorGUI::Shutdown() { LOG_I("EditorGUI Shutdown"); }

void EditorGUI::SetCurrentScene(std::shared_ptr<Scene> scene) {
  s_CurrentScene = scene;
  s_SelectedNode = nullptr; // 清除选择
}

void EditorGUI::Render() {
  // 设置DockSpace
  SetupDockSpace();

  // 渲染各个面板
  RenderMenuBar();

  // if (!s_CurrentScene) {
  //   // 显示欢迎界面
  //   ImGui::Begin("Welcome");
  //   ImGui::Text("No project loaded");
  //   ImGui::Separator();
  //   ImGui::Text("Please create a new project or load an existing one:");
  //   if (ImGui::Button("New Project")) {
  //     CreateNewProject();
  //   }
  //   ImGui::SameLine();
  //   if (ImGui::Button("Load Project")) {
  //     LoadProject();
  //   }
  //   ImGui::End();
  //   return;
  // }

  RenderSceneHierarchy();
  RenderInspector();
  RenderContentBrowser();

  // 关闭前的确认弹窗
  if (Window::IsCloseRequested()) {
    ImGui::OpenPopup("退出确认");
  }

  if (ImGui::BeginPopupModal("退出确认", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("您有未保存的更改吗？退出前是否保存工程？");
    ImGui::Separator();

    if (ImGui::Button("保存并退出", ImVec2(120, 40))) {
      SaveProject();
      Window::Close();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("直接退出", ImVec2(120, 40))) {
      Window::Close();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("取消", ImVec2(120, 40))) {
      Window::ResetCloseRequest();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void EditorGUI::SetupDockSpace() {
#ifdef IMGUI_HAS_DOCK
  static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

  // 使用全屏窗口作为DockSpace
  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

  ImGui::Begin("DockSpaceWindow", nullptr, window_flags);
  ImGui::PopStyleVar(3);

  // DockSpace
  ImGuiIO &io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

    // 首次初始化布局
    if (!s_DockSpaceInitialized) {
      s_DockSpaceInitialized = true;

      ImGui::DockBuilderRemoveNode(dockspace_id);
      ImGui::DockBuilderAddNode(dockspace_id,
                                dockspace_flags | ImGuiDockNodeFlags_DockSpace);
      ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

      // 分割布局
      auto dock_id_left = ImGui::DockBuilderSplitNode(
          dockspace_id, ImGuiDir_Left, 0.2f, nullptr, &dockspace_id);
      auto dock_id_right = ImGui::DockBuilderSplitNode(
          dockspace_id, ImGuiDir_Right, 0.25f, nullptr, &dockspace_id);
      auto dock_id_bottom = ImGui::DockBuilderSplitNode(
          dockspace_id, ImGuiDir_Down, 0.25f, nullptr, &dockspace_id);

      // 分配窗口到Dock节点
      ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_id_left);
      ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
      ImGui::DockBuilderDockWindow("Content Browser", dock_id_bottom);
      ImGui::DockBuilderDockWindow("Viewport", dockspace_id);

      ImGui::DockBuilderFinish(dockspace_id);
    }
  }

  ImGui::End();
#endif
}

void EditorGUI::RenderMenuBar() {
  if (ImGui::BeginMainMenuBar()) {
    MenuFile();
    MenuCreate();
    MenuDebug();
    MenuPerformance();

    // 右侧显示FPS
    ImGuiIO &io = ImGui::GetIO();
    float menuBarWidth = ImGui::GetWindowWidth();
    std::string fpsText = "FPS: " + std::to_string((int)io.Framerate);
    float textWidth = ImGui::CalcTextSize(fpsText.c_str()).x;
    ImGui::SetCursorPosX(menuBarWidth - textWidth - 10.0f);
    ImGui::Text("%s", fpsText.c_str());

    ImGui::EndMainMenuBar();
  }
}

void EditorGUI::MenuFile() {
  if (ImGui::BeginMenu("文件")) {
    // 工程管理
    if (ImGui::MenuItem("新项目...", "Ctrl+Shift+N")) {
      CreateNewProject();
    }
    if (ImGui::MenuItem("加载项目...", "Ctrl+Shift+O")) {
      LoadProject();
    }

    auto currentProject = RenderCore::GetCurrentProject();
    bool hasProject = (currentProject != nullptr);

    if (ImGui::MenuItem("保存项目", "Ctrl+Shift+S", false, hasProject)) {
      SaveProject();
    }

    ImGui::Separator();

    // 导入资源
    if (ImGui::MenuItem("导入模型...", nullptr, false, hasProject)) {
      std::string modelFile = ShowFileDialog(
          true, "Model Files (*.obj;*.gltf;*.glb)\0*.obj;*.gltf;*.glb\0All "
                "Files\0*.*\0");
      if (!modelFile.empty() && s_CurrentScene) {
        // Use ModelImporter to properly import and add to scene
        ModelImporter importer;
        auto result = importer.ImportToScene(
            modelFile, currentProject->GetAssetsPath(), *s_CurrentScene);
        if (result.success) {
          LOG_I("Imported model to scene: {} ({} meshes created)", modelFile,
                result.meshIDs.size());
        } else {
          LOG_E("Failed to import model: {}", result.errorMessage);
        }
      } else if (!s_CurrentScene) {
        LOG_W("No scene loaded, please create a scene first");
      }
    }

    if (ImGui::MenuItem("导入纹理...", nullptr, false, hasProject)) {
      std::string textureFile = ShowFileDialog(
          true, "Image Files (*.png;*.jpg;*.jpeg)\0*.png;*.jpg;*.jpeg\0All "
                "Files\0*.*\0");
      if (!textureFile.empty()) {
        std::filesystem::path src(textureFile);
        std::filesystem::path dst =
            std::filesystem::path(currentProject->GetAssetsPath()) /
            src.filename();
        try {
          std::filesystem::copy_file(
              src, dst, std::filesystem::copy_options::overwrite_existing);

          // 核心修复: 导入后立即注册资产
          AssetManager::GetInstance().RegisterAsset(dst, "texture");

          LOG_I("Imported and registered texture: {}", dst.u8string());
        } catch (const std::exception &e) {
          LOG_E("Failed to import texture: {}", e.what());
        }
      }
    }

    ImGui::Separator();

    if (ImGui::MenuItem("退出渲染器")) {
      Window::RequestClose();
    }

    ImGui::EndMenu();
  }
}

void EditorGUI::MenuCreate() {
  auto project = RenderCore::GetCurrentProject();
  bool hasProject = (project != nullptr);

  if (ImGui::BeginMenu("创建")) {
    // 场景
    if (ImGui::MenuItem("场景", nullptr, false, hasProject)) {
      CreateScene();
    }

    // 空物体
    if (ImGui::MenuItem("空物体", nullptr, false, s_CurrentScene != nullptr)) {
      CreateEmptyNode();
    }

    ImGui::Separator();

    // 3D Objects
    if (ImGui::BeginMenu("3D物体", s_CurrentScene != nullptr)) {
      if (ImGui::MenuItem("Cube")) {
        CreateCube();
      }
      if (ImGui::MenuItem("Sphere")) {
        CreateSphere();
      }
      if (ImGui::MenuItem("Plane")) {
        CreatePlane();
      }
      ImGui::EndMenu();
    }

    // Lights
    if (ImGui::BeginMenu("光源", s_CurrentScene != nullptr)) {
      if (ImGui::MenuItem("点光源")) {
        CreatePointLight();
      }
      ImGui::EndMenu();
    }

    // Camera
    if (ImGui::MenuItem("相机", nullptr, false, s_CurrentScene != nullptr)) {
      CreateCamera();
    }

    ImGui::EndMenu();
  }
}

void EditorGUI::MenuDebug() {
  if (ImGui::BeginMenu("Debug")) {
    auto &settings = RenderCore::GetPostProcessSettings();

    if (ImGui::MenuItem("Shaded", nullptr,
                        s_RenderMode == RenderMode::Shaded)) {
      s_RenderMode = RenderMode::Shaded;
      settings.debugMode = 0;
      LOG_I("Render mode: Shaded");
    }
    if (ImGui::MenuItem("Wireframe", nullptr,
                        s_RenderMode == RenderMode::Wireframe)) {
      s_RenderMode = RenderMode::Wireframe;
      settings.debugMode = 1;
      LOG_I("Render mode: Wireframe");
    }
    if (ImGui::MenuItem("Albedo", nullptr,
                        s_RenderMode == RenderMode::Albedo)) {
      s_RenderMode = RenderMode::Albedo;
      settings.debugMode = 2;
      LOG_I("Render mode: Albedo");
    }
    if (ImGui::MenuItem("Normal", nullptr,
                        s_RenderMode == RenderMode::Normal)) {
      s_RenderMode = RenderMode::Normal;
      settings.debugMode = 3;
      LOG_I("Render mode: Normal");
    }
    if (ImGui::MenuItem("Depth", nullptr, s_RenderMode == RenderMode::Depth)) {
      s_RenderMode = RenderMode::Depth;
      settings.debugMode = 4;
      LOG_I("Render mode: Depth");
    }
    if (ImGui::MenuItem("Smoothness", nullptr,
                        s_RenderMode == RenderMode::Smoothness)) {
      s_RenderMode = RenderMode::Smoothness;
      settings.debugMode = 5;
      LOG_I("Render mode: Smoothness");
    }
    if (ImGui::MenuItem("Specular", nullptr,
                        s_RenderMode == RenderMode::Specular)) {
      s_RenderMode = RenderMode::Specular;
      settings.debugMode = 6;
      LOG_I("Render mode: Specular");
    }
    if (ImGui::MenuItem("Occlusion", nullptr,
                        s_RenderMode == RenderMode::Occlusion)) {
      s_RenderMode = RenderMode::Occlusion;
      settings.debugMode = 7;
      LOG_I("Render mode: Occlusion");
    }
    if (ImGui::MenuItem("MaterialFlags", nullptr,
                        s_RenderMode == RenderMode::MaterialFlags)) {
      s_RenderMode = RenderMode::MaterialFlags;
      settings.debugMode = 8;
      LOG_I("Render mode: MaterialFlags");
    }
    if (ImGui::MenuItem("ShadingID", nullptr,
                        s_RenderMode == RenderMode::ShadingID)) {
      s_RenderMode = RenderMode::ShadingID;
      settings.debugMode = 9;
      LOG_I("Render mode: ShadingID");
    }
    if (ImGui::MenuItem("Emission", nullptr,
                        s_RenderMode == RenderMode::Emission)) {
      s_RenderMode = RenderMode::Emission;
      settings.debugMode = 10;
      LOG_I("Render mode: Emission");
    }

    ImGui::EndMenu();
  }
}

void EditorGUI::MenuPerformance() {
  if (ImGui::BeginMenu("性能")) {
    bool vsync = RenderCore::IsVSyncEnabled();
    if (ImGui::MenuItem("垂直同步 (VSync)", nullptr, &vsync)) {
      RenderCore::SetVSync(vsync);
      LOG_I("VSync {}", vsync ? "Enabled" : "Disabled");
    }

    int targetFPS = RenderCore::GetTargetFPS();
    const char *fpsOptions[] = {"不限制",  "30 FPS",  "60 FPS",
                                "120 FPS", "144 FPS", "165 FPS"};
    int fpsValues[] = {0, 30, 60, 120, 144, 165};
    int currentIdx = 0;
    for (int i = 0; i < 6; i++) {
      if (targetFPS == fpsValues[i]) {
        currentIdx = i;
        break;
      }
    }

    if (ImGui::Combo("目标帧率", &currentIdx, fpsOptions,
                     IM_ARRAYSIZE(fpsOptions))) {
      RenderCore::SetTargetFPS(fpsValues[currentIdx]);
      LOG_I("Target FPS set to: {}",
            fpsValues[currentIdx] == 0 ? "Unlimited"
                                       : std::to_string(fpsValues[currentIdx]));
    }

    ImGui::Separator();

    // TAA Settings
    bool taa = RenderCore::IsTAAEnabled();
    if (ImGui::Checkbox("Enable TAA", &taa)) {
      RenderCore::SetTAAEnabled(taa);
    }

    if (taa) {
      float feedback = RenderCore::GetTAAFeedbackFactor();
      if (ImGui::SliderFloat("TAA Feedback", &feedback, 0.0f, 0.99f)) {
        RenderCore::SetTAAFeedbackFactor(feedback);
      }
    }

    // Super Resolution
    float scale = RenderCore::GetSuperResolutionScale();
    if (ImGui::SliderFloat("Super Resolution Scale", &scale, 1.0f, 2.0f,
                           "%.2f")) {
      RenderCore::SetSuperResolutionScale(scale);
    }

    if (ImGui::Button("保存设置并重启以应用修改")) {
      // 1. Save Global Settings
      RenderCore::SaveGlobalSettings();

      // 2. Save Project (if user wants to savescene changes too)
      SaveProject();

      // 3. Restart
      auto project = RenderCore::GetCurrentProject();
      if (project) {
        LOG_I("Restarting application to apply resolution changes...");
        // Pass project path as argument to reload it on startup
        Window::Restart(project->GetProjectPath().c_str());
        Window::Close();
      } else {
        Window::Restart(nullptr);
      }
    }

    ImGui::Separator();
    ImGui::Text("当前 FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("帧时间: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);

    ImGui::EndMenu();
  }
}

void EditorGUI::RenderSceneHierarchy() {
  ImGui::Begin("场景层级");

  // 处理挂起的节点操作
  // (在遍历之前或之后，这里选在之前，但要注意之前遍历可能未完成。 实际上在
  // RenderSceneHierarchy 的开头处理上一帧或本帧还未开始遍历时的操作最安全)
  // 或者在末尾。

  auto project = RenderCore::GetCurrentProject();
  if (!project) {
    ImGui::TextDisabled("无项目加载");
    ImGui::End();
    return;
  }

  // World root node (implicit)
  ImGuiTreeNodeFlags worldFlags = ImGuiTreeNodeFlags_OpenOnArrow |
                                  ImGuiTreeNodeFlags_DefaultOpen |
                                  ImGuiTreeNodeFlags_SpanAvailWidth;

  if (ImGui::TreeNodeEx("World", worldFlags)) {
    // Show all scenes
    for (size_t i = 0; i < s_Scenes.size(); i++) {
      auto &scene = s_Scenes[i];

      ImGuiTreeNodeFlags sceneFlags =
          ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

      // Highlight active scene
      if (static_cast<int>(i) == s_ActiveSceneIndex) {
        sceneFlags |= ImGuiTreeNodeFlags_Selected;
      }

      // Scene icon and name
      std::string sceneLabel = "[S] " + scene->GetName();
      bool sceneOpen = ImGui::TreeNodeEx((void *)(intptr_t)i, sceneFlags, "%s",
                                         sceneLabel.c_str());

      // Click to select scene
      if (ImGui::IsItemClicked()) {
        s_ActiveSceneIndex = static_cast<int>(i);
        s_CurrentScene = scene;
        s_SelectedNode = nullptr;
      }

      // Right-click context menu for scenes
      if (ImGui::BeginPopupContextItem(
              ("SceneContext##" + std::to_string(i)).c_str())) {
        if (ImGui::MenuItem("删除场景")) {
          // Delete the scene
          std::string sceneName = scene->GetName();
          s_Scenes.erase(s_Scenes.begin() + i);

          // Update active scene index
          if (s_ActiveSceneIndex == static_cast<int>(i)) {
            // If we deleted the active scene, select another one
            if (!s_Scenes.empty()) {
              s_ActiveSceneIndex = min(s_ActiveSceneIndex,
                                       static_cast<int>(s_Scenes.size()) - 1);
              s_CurrentScene = s_Scenes[s_ActiveSceneIndex];
            } else {
              s_ActiveSceneIndex = -1;
              s_CurrentScene = nullptr;
            }
            s_SelectedNode = nullptr;
          } else if (s_ActiveSceneIndex > static_cast<int>(i)) {
            // Adjust index if we deleted a scene before the active one
            s_ActiveSceneIndex--;
          }

          LOG_I("Deleted scene: {}", sceneName);
          ImGui::EndPopup();
          if (sceneOpen) {
            ImGui::TreePop();
          }
          break; // Exit the loop since we modified the vector
        }
        ImGui::EndPopup();
      }

      if (sceneOpen) {
        // Show scene's nodes
        Node *rootNode = scene->GetRootNode();
        if (rootNode) {
          for (const auto &child : rootNode->GetChildren()) {
            RenderNodeTree(child.get());
          }
        }
        ImGui::TreePop();
      }
    }

    // Show message if no scenes
    if (s_Scenes.empty()) {
      ImGui::TextDisabled("无场景 - 在创建菜单中创建一个");
    }

    ImGui::TreePop();
  }

  // Right-click context menu on empty space
  if (ImGui::BeginPopupContextWindow("场景层级上下文",
                                     ImGuiPopupFlags_MouseButtonRight |
                                         ImGuiPopupFlags_NoOpenOverItems)) {
    if (ImGui::MenuItem("创建场景")) {
      CreateScene();
    }
    if (s_CurrentScene) {
      if (ImGui::MenuItem("创建空物体")) {
        CreateEmptyNode();
      }
    }
    ImGui::EndPopup();
  }

  ImGui::End();

  // 在本次面板渲染结束时处理所有结构性变更
  ProcessPendingNodeOperations();
}

void EditorGUI::ProcessPendingNodeOperations() {
  if (s_PendingReparentSource && s_PendingReparentTarget) {
    Node *parent = s_PendingReparentSource->GetParent();
    if (parent) {
      s_PendingReparentTarget->AddChild(
          parent->RemoveChild(s_PendingReparentSource));
    }
    s_PendingReparentSource = nullptr;
    s_PendingReparentTarget = nullptr;
  }

  if (s_PendingCloneSource) {
    auto copy = s_PendingCloneSource->Clone();
    if (copy) {
      Node *parent = s_PendingCloneSource->GetParent();
      if (parent) {
        parent->AddChild(std::move(copy));
      }
    }
    s_PendingCloneSource = nullptr;
  }

  if (s_PendingDeleteNode) {
    Node *parent = s_PendingDeleteNode->GetParent();
    if (parent) {
      if (s_SelectedNode == s_PendingDeleteNode)
        s_SelectedNode = nullptr;
      parent->RemoveChild(s_PendingDeleteNode);
    }
    s_PendingDeleteNode = nullptr;
  }
}

void EditorGUI::RenderNodeTree(Node *node) {
  if (!node)
    return;

  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                             ImGuiTreeNodeFlags_OpenOnDoubleClick |
                             ImGuiTreeNodeFlags_SpanAvailWidth;

  // 如果是选中的节点，高亮显示
  if (node == s_SelectedNode) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }

  // 如果没有子节点，显示为叶子节点
  bool isLeaf = node->GetChildren().empty();
  if (isLeaf) {
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  }

  // 节点图标（根据类型）
  const char *icon = "[ ]";
  std::string nodeType = node->GetNodeType();
  if (nodeType == "MeshNode") {
    icon = "[M]";
  } else if (nodeType == "LightNode") {
    icon = "[L]";
  } else if (nodeType == "CameraNode") {
    icon = "[C]";
  }

  // 渲染节点
  std::string label = std::string(icon) + " " + node->GetName();
  bool nodeOpen = ImGui::TreeNodeEx((void *)(intptr_t)node->GetUUID(), flags,
                                    "%s", label.c_str());

  // 点击选择
  if (ImGui::IsItemClicked()) {
    s_SelectedNode = node;
  }

  // 拖拽源
  if (ImGui::BeginDragDropSource()) {
    ImGui::SetDragDropPayload("SCENE_NODE", &node, sizeof(Node *));
    ImGui::Text("Move %s", node->GetName().c_str());
    ImGui::EndDragDropSource();
  }

  // 拖拽目标
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload *payload =
            ImGui::AcceptDragDropPayload("SCENE_NODE")) {
      Node *draggedNode = *(Node **)payload->Data;
      if (draggedNode != node) {
        // 循环嵌套检查：被拖拽的节点不能是目标节点的父级或祖先
        auto isDescendantOf = [](Node *potentialParent, Node *potentialChild) {
          Node *parent = potentialChild->GetParent();
          while (parent) {
            if (parent == potentialParent)
              return true;
            parent = parent->GetParent();
          }
          return false;
        };

        if (!isDescendantOf(draggedNode, node)) {
          LOG_I("Request reparenting {0} to {1}", draggedNode->GetName(),
                node->GetName());
          s_PendingReparentSource = draggedNode;
          s_PendingReparentTarget = node;
        } else {
          LOG_W("Cannot reparent node to its own descendant: {}",
                draggedNode->GetName());
        }
      }
    }
    ImGui::EndDragDropTarget();
  }

  // 右键菜单
  if (ImGui::BeginPopupContextItem()) {
    if (ImGui::MenuItem("删除")) {
      DeleteSelectedNode();
    }
    if (ImGui::MenuItem("复制")) {
      s_PendingCloneSource = node;
    }
    ImGui::EndPopup();
  }

  // 递归渲染子节点
  if (nodeOpen && !isLeaf) {
    for (const auto &child : node->GetChildren()) {
      RenderNodeTree(child.get());
    }
    ImGui::TreePop();
  }
}

void EditorGUI::RenderInspector() {
  ImGui::Begin("详细信息");

  if (s_SelectedNode) {
    // 顶部：对象名称和UUID
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::BeginChild("InspectorHeader", ImVec2(0, 120), true);

    // 对象名称（可编辑）
    char nameBuffer[256];
    strncpy(nameBuffer, s_SelectedNode->GetName().c_str(), sizeof(nameBuffer));
    nameBuffer[sizeof(nameBuffer) - 1] = '\0';
    if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer))) {
      s_SelectedNode->SetName(nameBuffer);
    }

    // UUID（只读）
    ImGui::Text("UUID: %s", s_SelectedNode->GetUUID().ToString().c_str());

    // 激活状态
    bool isActive = s_SelectedNode->IsActive();
    if (ImGui::Checkbox("Active", &isActive)) {
      s_SelectedNode->SetActive(isActive);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::Separator();
  } else {
    ImGui::TextDisabled("No object selected");
    ImGui::Separator();
  }

  // 使用水平分割：左侧Tab栏，右侧内容
  ImGui::BeginChild("InspectorContent", ImVec2(0, 0), false);

  ImGui::Columns(2, "InspectorColumns", true);
  ImGui::SetColumnWidth(0, 120.0f);

  // 左侧：纵向Tab栏
  ImGui::BeginChild("TabBar", ImVec2(0, 0), true);

  // World Tab（常驻）
  if (VerticalTab("World", s_CurrentInspectorTab == 0, ImVec2(100.0f, 40.0f))) {
    s_CurrentInspectorTab = 0;
  }

  // View Tab（常驻）
  if (VerticalTab("View", s_CurrentInspectorTab == 7, ImVec2(100.0f, 40.0f))) {
    s_CurrentInspectorTab = 7;
  }

  // PostProcess Tab（常驻）
  if (VerticalTab("PostProcess", s_CurrentInspectorTab == 5,
                  ImVec2(100.0f, 40.0f))) {
    s_CurrentInspectorTab = 5;
  }

  // Transform Tab
  if (s_SelectedNode) {
    if (VerticalTab("Transform", s_CurrentInspectorTab == 1,
                    ImVec2(100.0f, 40.0f))) {
      s_CurrentInspectorTab = 1;
    }

    // 根据节点类型显示不同Tab
    std::string nodeType = s_SelectedNode->GetNodeType();
    if (nodeType == "MeshNode") {
      if (VerticalTab("Mesh", s_CurrentInspectorTab == 2,
                      ImVec2(100.0f, 40.0f))) {
        s_CurrentInspectorTab = 2;
      }
      if (VerticalTab("Material", s_CurrentInspectorTab == 3,
                      ImVec2(100.0f, 40.0f))) {
        s_CurrentInspectorTab = 3;
      }
    } else if (nodeType == "PointLightNode" || nodeType == "LightNode") {
      if (VerticalTab("Light", s_CurrentInspectorTab == 4,
                      ImVec2(100.0f, 40.0f))) {
        s_CurrentInspectorTab = 4;
      }
    } else if (nodeType == "CameraNode") {
      if (VerticalTab("Camera", s_CurrentInspectorTab == 6,
                      ImVec2(100.0f, 40.0f))) {
        s_CurrentInspectorTab = 6;
      }
    }
  }

  ImGui::EndChild();

  // 右侧：属性编辑区
  ImGui::NextColumn();
  ImGui::BeginChild("Properties", ImVec2(0, 0), true);

  switch (s_CurrentInspectorTab) {
  case 0: // World
  {
    ImGui::Text("世界环境设置");
    ImGui::Separator();

    // ========== 平行光设置 ==========
    ImGui::Text("平行光设置");
    ImGui::Separator();

    auto &pcssSettings = RenderCore::GetPCSSSettings();

    // 平行光开关
    bool enableDirLight = pcssSettings.enableDirectionalLight;
    if (ImGui::Checkbox("启用平行光", &enableDirLight)) {
      pcssSettings.enableDirectionalLight = enableDirLight;
      LOG_I("enableDirLight: {}", enableDirLight);
    }

    // 阴影开关
    bool enableShadow = pcssSettings.enableShadow;
    if (ImGui::Checkbox("启用阴影", &enableShadow)) {
      pcssSettings.enableShadow = enableShadow;
      LOG_I("enableShadow: {}", enableShadow);
    }

    ImGui::Spacing();

    // 光源方向
    glm::vec3 lightDir = pcssSettings.lightDirection;
    if (ImGui::DragFloat3("光源方向", &lightDir.x, 0.01f, -1.0f, 1.0f)) {
      if (glm::length(lightDir) > 0.001f) {
        pcssSettings.lightDirection = glm::normalize(lightDir);
      }
    }

    // 光源大小
    float lightSize = pcssSettings.lightSize;
    if (ImGui::SliderFloat("光源大小", &lightSize, 0.1f, 100.0f)) {
      pcssSettings.lightSize = lightSize;
    }

    // 阴影距离
    float shadowDist = pcssSettings.shadowDistance;
    if (ImGui::SliderFloat("阴影距离", &shadowDist, 10.0f, 200.0f)) {
      pcssSettings.shadowDistance = shadowDist;
    }

    // 阴影偏移 (Bias)
    float shadowBias = pcssSettings.bias;
    if (ImGui::SliderFloat("阴影偏移", &shadowBias, -0.003001f, 0.003001f,
                           "%.6f")) {
      pcssSettings.bias = shadowBias;
    }

    // 基础模糊
    float minFilter = pcssSettings.minFilterSize;
    if (ImGui::SliderFloat("基础模糊(px)", &minFilter, 0.0f, 10.0f)) {
      pcssSettings.minFilterSize = minFilter;
    }

    ImGui::Spacing();
    ImGui::Text("阴影质量设置");
    ImGui::Separator();

    // 遮挡物采样数
    int blockerSamples = static_cast<int>(pcssSettings.blockerSamples);
    if (ImGui::SliderInt("遮挡物采样数", &blockerSamples, 8, 32)) {
      pcssSettings.blockerSamples = static_cast<uint32_t>(blockerSamples);
      LOG_I("blockerSamples: {}", blockerSamples);
    }

    // PCF采样数
    int pcfSamples = static_cast<int>(pcssSettings.pcfSamples);
    if (ImGui::SliderInt("PCF采样数", &pcfSamples, 16, 64)) {
      pcssSettings.pcfSamples = static_cast<uint32_t>(pcfSamples);
      LOG_I("pcfSamples: {}", pcfSamples);
    }

    // 阴影贴图分辨率
    const char *resOptions[] = {"1024", "2048", "4096"};
    int currentResIdx = 1; // 默认2048
    if (pcssSettings.shadowMapRes == 1024)
      currentResIdx = 0;
    else if (pcssSettings.shadowMapRes == 2048)
      currentResIdx = 1;
    else if (pcssSettings.shadowMapRes == 4096)
      currentResIdx = 2;

    if (ImGui::Combo("阴影贴图分辨率", &currentResIdx, resOptions, 3)) {
      uint32_t newRes = 2048;
      if (currentResIdx == 0)
        newRes = 1024;
      else if (currentResIdx == 1)
        newRes = 2048;
      else if (currentResIdx == 2)
        newRes = 4096;

      if (newRes != pcssSettings.shadowMapRes) {
        pcssSettings.shadowMapRes = newRes;
        RenderCore::SetShadowMapResolution(newRes);
      }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("天空盒设置 (Skybox/IBL)");
    ImGui::Separator();

    auto &skybox = RenderCore::GetSkyboxSettings();
    if (skybox.name.empty()) {
      skybox.name = "Unnamed Skybox";
    }

    // 基本属性设置
    char skyboxName[256];
    strncpy(skyboxName, skybox.name.c_str(), sizeof(skyboxName));
    if (ImGui::InputText("名称", skyboxName, sizeof(skyboxName))) {
      skybox.name = skyboxName;
    }

    ImGui::SliderFloat("随Y轴旋转角度", &skybox.rotationY, 0.0f, 360.0f);
    ImGui::SliderFloat("天空盒亮度", &skybox.brightness, 0.0f, 10.0f);

    // 六个面拖放预览槽位
    const char *faceNames[6] = {"Right (PosX)",  "Left (NegX)",  "Top (PosY)",
                                "Bottom (NegY)", "Front (PosZ)", "Back (NegZ)"};
    if (skybox.facePaths.size() != 6) {
      skybox.facePaths.resize(6, "");
    }

    for (int i = 0; i < 6; i++) {
      RenderCubeFaceSlot(faceNames[i], skybox.facePaths[i], i);
    }

    ImGui::Spacing();

    // 操作按钮
    if (ImGui::Button("保存为 .skybox", ImVec2(-1, 0))) {
      // TODO: 使用文件选择器或直接保存到默认路径
      // 假设临时保存为 resource/textures/default.skybox
      skybox.SaveToFile("resource/textures/default.skybox");
      LOG_I("Skybox saved to resource/textures/default.skybox");
    }

    if (ImGui::Button("加载 .skybox", ImVec2(-1, 0))) {
      // TODO: 使用文件选择器加载
      if (skybox.LoadFromFile("resource/textures/default.skybox")) {
        LOG_I("Loaded skybox: {}", skybox.name);
      } else {
        LOG_E("Failed to load skybox");
      }
    }

    if (ImGui::Button("清空天空盒", ImVec2(-1, 0))) {
      skybox = SkyboxSettings();
      LOG_I("Cleared skybox settings");
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    if (ImGui::Button("刷新天空盒并进行预计算 (IBL)", ImVec2(-1, 30))) {
      // 触发CubeMap加载和SH系数生成
      RenderCore::ReloadSkybox();
      LOG_I("Refreshing Skybox via RenderCore::ReloadSkybox()");
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Fog (TODO)");
  } break;

  case 7: // Camera View settings (Permanent)
  {
    ImGui::Text("Editor Camera Settings");
    ImGui::Separator();

    auto &camera = RenderCore::GetCamera();
    float speed = camera.GetMovementSpeed();
    float sensitivity = camera.GetMouseSensitivity();
    float fov = camera.GetFov();
    float nearPlane = camera.GetNearPlane();
    float farPlane = camera.GetFarPlane();

    if (ImGui::DragFloat("Movement Speed", &speed, 0.1f, 0.0f, 100.0f))
      camera.SetMovementSpeed(speed);
    if (ImGui::DragFloat("Mouse Sensitivity", &sensitivity, 0.001f, 0.001f,
                         1.0f))
      camera.SetMouseSensitivity(sensitivity);
    if (ImGui::DragFloat("FOV", &fov, 1.0f, 1.0f, 120.0f))
      camera.SetFov(fov);
    if (ImGui::DragFloat("Near Plane", &nearPlane, 0.01f, 0.01f, 10.0f))
      camera.SetNearPlane(nearPlane);
    if (ImGui::DragFloat("Far Plane", &farPlane, 1.0f, 10.0f, 10000.0f))
      camera.SetFarPlane(farPlane);

    if (ImGui::Button("Restore Defaults")) {
      camera.SetMovementSpeed(2.5f);
      camera.SetMouseSensitivity(0.1f);
      camera.SetFov(45.0f);
      camera.SetNearPlane(0.1f);
      camera.SetFarPlane(100.0f);
    }
  } break;

  case 1: // Transform
    if (s_SelectedNode)
      RenderTransformEditor(s_SelectedNode);
    break;

  case 2: // Mesh
    if (s_SelectedNode && s_SelectedNode->GetNodeType() == "MeshNode") {
      RenderMeshNodeInspector(s_SelectedNode);
    }
    break;

  case 3: // Material
    if (s_SelectedNode && s_SelectedNode->GetNodeType() == "MeshNode") {
      ImGui::Text("Material Properties");
      ImGui::Separator();
      // ... content continues ...

      // 获取 MeshNode 的材质
      auto *meshNode = static_cast<MeshNode *>(s_SelectedNode);
      UUID materialID = meshNode->GetMaterialID();
      MaterialResource *matRes = nullptr;

      if (materialID.IsValid()) {
        matRes = RenderCore::GetMaterialResource(materialID);
      }

      // Material Selection / Creation
      if (!matRes || !materialID.IsValid()) {
        ImGui::TextDisabled("No Material Assigned");
        if (ImGui::Button("Create New Material")) {
          UUID newMatID = RenderCore::CreateMaterial();
          meshNode->SetMaterialID(newMatID);
        }
        ImGui::SameLine();
        if (ImGui::Button("Select Material")) {
          ImGui::OpenPopup("SelectMaterialPopup");
        }
      } else {
        ImGui::Text("Material: %s", matRes->GetName().c_str());
        ImGui::TextDisabled("UUID: %s", materialID.ToString().c_str());

        if (ImGui::Button("Change Material")) {
          ImGui::OpenPopup("SelectMaterialPopup");
        }
        ImGui::SameLine();
        if (ImGui::Button("Unassign Material")) {
          meshNode->SetMaterialID(UUID::Invalid());
          matRes = nullptr;
        }
      }

      // Material Selection Popup
      if (ImGui::BeginPopup("SelectMaterialPopup")) {
        std::vector<UUID> materials = RenderCore::GetAllMaterials();
        for (const auto &id : materials) {
          MaterialResource *m = RenderCore::GetMaterialResource(id);
          std::string label = m->GetName() + "##" + id.ToString();
          if (ImGui::Selectable(label.c_str())) {
            meshNode->SetMaterialID(id);
          }
        }
        ImGui::EndPopup();
      }

      if (matRes) {
        ImGui::Separator();
        // Material Name
        char nameBuf[256];
        strncpy(nameBuf, matRes->GetName().c_str(), sizeof(nameBuf));
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
          matRes->SetName(nameBuf);
          matRes->material.name = nameBuf;
        }

        ImGui::Separator();

        // PBR 属性
        // 基础颜色 (Base Color)
        if (ImGui::CollapsingHeader("Base Color",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
          // 颜色编辑控件：允许用户调整材质的颜色因子 (RGBA)
          if (ImGui::ColorEdit4("Color Factor",
                                &matRes->material.baseColorFactor.x)) {
            // 自动透明度切换逻辑：
            // 如果 Alpha 值小于 0.999，自动将材质类型切换为透明
            // (Transparent)，使用前向渲染
            if (matRes->material.baseColorFactor.a < 0.999f) {
              if (matRes->material.type != MaterialType::Transparent) {
                matRes->material.type = MaterialType::Transparent;
                matRes->material.alphaMode = "BLEND";
              }
            } else {
              // 否则切换为不透明 (Opaque)，使用延迟渲染
              if (matRes->material.type != MaterialType::Opaque) {
                matRes->material.type = MaterialType::Opaque;
                matRes->material.alphaMode = "OPAQUE";
              }
            }
            // 同步更新材质的 alpha 属性，确保与 baseColorFactor.a 保持一致
            matRes->material.alpha = matRes->material.baseColorFactor.a;
          }

          // 显示当前的渲染路径类型
          ImGui::Text("Render Type: %s",
                      (matRes->material.type == MaterialType::Transparent)
                          ? "Transparent (Forward)"
                          : "Opaque (Deferred)");

          // Base Color Map
          RenderTextureSlot("Base Color Map", materialID, 0,
                            matRes->baseColorTex,
                            matRes->material.baseColorTexture);
        }

        // Metallic / Roughness
        if (ImGui::CollapsingHeader("Metallic / Roughness",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
          ImGui::SliderFloat("Metallic", &matRes->material.metallicFactor, 0.0f,
                             1.0f);
          ImGui::SliderFloat("Roughness", &matRes->material.roughnessFactor,
                             0.0f, 1.0f);

          // Metallic Map
          RenderTextureSlot("Metallic Map", materialID, 1, matRes->metallicTex,
                            matRes->material.metallicTexture);

          // Roughness Map
          RenderTextureSlot("Roughness Map", materialID, 5,
                            matRes->roughnessTex,
                            matRes->material.roughnessTexture);
        }

        // Normal Map
        if (ImGui::CollapsingHeader("Normal Map")) {
          ImGui::SliderFloat("Scale", &matRes->material.normalScale, 0.0f,
                             2.0f);
          // Normal Map
          RenderTextureSlot("Normal Map", materialID, 2, matRes->normalTex,
                            matRes->material.normalTexture);
        }

        // Emission
        if (ImGui::CollapsingHeader("Emission")) {
          ImGui::DragFloat("Intensity", &matRes->material.emissiveIntensity,
                           0.1f, 0.0f, 100.0f);
          // Emissive Map
          RenderTextureSlot("Emissive Map", materialID, 3, matRes->emissiveTex,
                            matRes->material.emissiveTexture);
        }

        // Occlusion
        if (ImGui::CollapsingHeader("Occlusion")) {
          // Occlusion Map
          RenderTextureSlot("Occlusion Map", materialID, 4,
                            matRes->occlusionTex,
                            matRes->material.occlusionTexture);
        }
      }
    }
    break;

  case 4: // Light
    if (s_SelectedNode && s_SelectedNode->GetNodeType() == "PointLightNode") {
      RenderPointLightInspector(static_cast<PointLightNode *>(s_SelectedNode));
    }
    break;

  case 6: // Camera
    if (s_SelectedNode && s_SelectedNode->GetNodeType() == "CameraNode") {
      RenderCameraNodeInspector(s_SelectedNode);
    }
    break;

  case 5: // PostProcess
    RenderPostProcessInspector();
    break;
  }

  ImGui::EndChild();
  ImGui::Columns(1);

  ImGui::EndChild();

  ImGui::End();
}

void EditorGUI::RenderTransformEditor(Node *node) {
  if (!node)
    return;

  ImGui::Text("Transform");
  ImGui::Separator();

  // Position
  glm::vec3 position = node->GetPosition();
  if (ImGui::DragFloat3("Position", &position.x, 0.1f)) {
    node->SetPosition(position);
  }

  // Rotation (欧拉角，度)
  glm::vec3 rotation = node->GetRotation();
  if (ImGui::DragFloat3("Rotation", &rotation.x, 1.0f)) {
    node->SetRotation(rotation);
  }

  // Scale
  glm::vec3 scale = node->GetScale();
  if (ImGui::DragFloat3("Scale", &scale.x, 0.1f, 0.01f, 100.0f)) {
    node->SetScale(scale);
  }
}

void EditorGUI::RenderMeshNodeInspector(Node *node) {
  ImGui::Text("Mesh Properties");
  ImGui::Separator();

  // TODO: 显示网格信息
  ImGui::Text("Mesh: (TODO)");
  ImGui::Text("Vertices: 0");
  ImGui::Text("Triangles: 0");
}

void EditorGUI::RenderCameraNodeInspector(Node *node) {
  if (!node || node->GetNodeType() != "CameraNode")
    return;
  auto *camNode = static_cast<CameraNode *>(node);

  ImGui::Text("Camera Properties");
  ImGui::Separator();

  float speed = camNode->GetMovementSpeed();
  if (ImGui::DragFloat("Movement Speed", &speed, 0.1f, 0.0f, 100.0f))
    camNode->SetMovementSpeed(speed);

  float sensitivity = camNode->GetMouseSensitivity();
  if (ImGui::DragFloat("Mouse Sensitivity", &sensitivity, 0.01f, 0.01f, 1.0f))
    camNode->SetMouseSensitivity(sensitivity);

  float fov = camNode->GetFov();
  if (ImGui::DragFloat("FOV", &fov, 1.0f, 1.0f, 120.0f))
    camNode->SetFov(fov);

  float nearPlane = camNode->GetNearPlane();
  if (ImGui::DragFloat("Near Plane", &nearPlane, 0.01f, 0.01f, 10.0f))
    camNode->SetNearPlane(nearPlane);

  float farPlane = camNode->GetFarPlane();
  if (ImGui::DragFloat("Far Plane", &farPlane, 1.0f, 10.0f, 10000.0f))
    camNode->SetFarPlane(farPlane);

  ImGui::Separator();

  if (ImGui::Button("记录当前视角", ImVec2(-1, 0))) {
    auto &mainCam = RenderCore::GetCamera();
    camNode->SetPosition(mainCam.GetPosition());

    // Convert yaw/pitch to rotation for the node if needed,
    // but Node uses Euler angles. Camera uses yaw/pitch.
    // For simplicity, we can just store the orientation.
    // However, Node's rotation is YXZ.
    camNode->SetRotation(
        glm::vec3(-mainCam.GetPitch(), -mainCam.GetYaw() - 90.0f, 0.0f));

    camNode->SetFov(mainCam.GetFov());
    camNode->SetNearPlane(mainCam.GetNearPlane());
    camNode->SetFarPlane(mainCam.GetFarPlane());
    camNode->SetMovementSpeed(mainCam.GetMovementSpeed());
    camNode->SetMouseSensitivity(mainCam.GetMouseSensitivity());
    LOG_I("Aligned CameraNode to current view");
  }

  if (ImGui::Button("对齐视角到相机", ImVec2(-1, 0))) {
    auto &mainCam = RenderCore::GetCamera();
    mainCam.SetPosition(camNode->GetPosition());

    // Set yaw/pitch from node rotation
    glm::vec3 rot = camNode->GetRotation();
    mainCam.SetYaw(-rot.y - 90.0f);
    mainCam.SetPitch(-rot.x);

    mainCam.SetFov(camNode->GetFov());
    mainCam.SetNearPlane(camNode->GetNearPlane());
    mainCam.SetFarPlane(camNode->GetFarPlane());
    mainCam.SetMovementSpeed(camNode->GetMovementSpeed());
    mainCam.SetMouseSensitivity(camNode->GetMouseSensitivity());
    LOG_I("Aligned current view to CameraNode");
  }
}

void EditorGUI::RenderPointLightInspector(PointLightNode *light) {
  ImGui::Text("Point Light Properties");
  ImGui::Separator();

  // Color
  glm::vec3 color = light->GetColor();
  float colorArr[3] = {color.r, color.g, color.b};
  if (ImGui::ColorEdit3("Color", colorArr)) {
    light->SetColor(glm::vec3(colorArr[0], colorArr[1], colorArr[2]));
  }

  // Intensity
  float intensity = light->GetIntensity();
  if (ImGui::DragFloat("Intensity", &intensity, 0.1f, 0.0f, 100.0f)) {
    light->SetIntensity(intensity);
  }

  // Radius
  float radius = light->GetRadius();
  if (ImGui::DragFloat("Radius", &radius, 0.5f, 0.1f, 1000.0f)) {
    light->SetRadius(radius);
  }

  ImGui::Separator();
  ImGui::Text("Attenuation");

  // Constant Attenuation
  float constant = light->GetConstantAttenuation();
  if (ImGui::DragFloat("Constant", &constant, 0.01f, 0.0f, 10.0f)) {
    light->SetConstantAttenuation(constant);
  }

  // Linear Attenuation
  float linear = light->GetLinearAttenuation();
  if (ImGui::DragFloat("Linear", &linear, 0.001f, 0.0f, 1.0f)) {
    light->SetLinearAttenuation(linear);
  }

  // Quadratic Attenuation
  float quadratic = light->GetQuadraticAttenuation();
  if (ImGui::DragFloat("Quadratic", &quadratic, 0.001f, 0.0f, 1.0f)) {
    light->SetQuadraticAttenuation(quadratic);
  }
}

void EditorGUI::RenderPostProcessInspector() {
  ImGui::Text("Post-Processing Settings");
  ImGui::Separator();

  auto &settings = RenderCore::GetPostProcessSettings();

  // SSAO Settings
  if (ImGui::CollapsingHeader("SSAO", ImGuiTreeNodeFlags_DefaultOpen)) {
    bool enableSSAO = settings.enableSSAO != 0;
    if (ImGui::Checkbox("Enable SSAO", &enableSSAO)) {
      settings.enableSSAO = enableSSAO ? 1 : 0;
    }
    ImGui::DragFloat("SSAO Radius", &settings.ssaoRadius, 0.01f, 0.01f, 5.0f);
    ImGui::DragFloat("SSAO Strength", &settings.ssaoStrength, 0.1f, 0.1f,
                     10.0f);
  }

  // Bloom Settings
  if (ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen)) {
    bool enableBloom = settings.enableBloom != 0;
    if (ImGui::Checkbox("Enable Bloom", &enableBloom)) {
      settings.enableBloom = enableBloom ? 1 : 0;
    }
    ImGui::DragFloat("Intensity##Bloom", &settings.bloomIntensity, 0.01f, 0.0f,
                     10.0f, "%.2f");
    ImGui::DragFloat("Threshold##Bloom", &settings.bloomThreshold, 0.01f, 0.0f,
                     10.0f, "%.2f");
    ImGui::DragFloat("Radius##Bloom", &settings.bloomRadius, 0.05f, 0.1f, 5.0f,
                     "%.2f");
  }

  // Tonemapping & Gamma
  if (ImGui::CollapsingHeader("Tonemapping & Color",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    bool enableToneMapping = settings.enableToneMapping != 0;
    if (ImGui::Checkbox("Enable Tone Mapping", &enableToneMapping)) {
      settings.enableToneMapping = enableToneMapping ? 1 : 0;
    }

    bool enableGamma = settings.enableGamma != 0;
    if (ImGui::Checkbox("Enable Gamma Correction", &enableGamma)) {
      settings.enableGamma = enableGamma ? 1 : 0;
    }
  }
}

/**
 * @brief 渲染内容浏览器面板
 * 显示项目资产目录，支持文件浏览、拖拽导入模型、创建文件夹等操作
 */
void EditorGUI::RenderContentBrowser() {
  ImGui::Begin("资产浏览器");

  // 获取当前项目，如果没有项目加载则显示提示
  auto project = RenderCore::GetCurrentProject();
  if (!project) {
    ImGui::TextDisabled("无项目加载");
    ImGui::End();
    return;
  }

  std::string assetsPath = project->GetAssetsPath();

  // 如果当前路径为空或不在资产目录下，初始化为资产根目录
  if (s_CurrentPath.empty() ||
      s_CurrentPath.find(assetsPath) == std::string::npos) {
    s_CurrentPath = assetsPath;
  }

  // 显示相对于 Assets 目录的当前路径
  std::string relativePath = s_CurrentPath;
  if (s_CurrentPath.length() > assetsPath.length()) {
    relativePath = "Assets" + s_CurrentPath.substr(assetsPath.length());
  } else {
    relativePath = "Assets";
  }
  ImGui::Text("Path: %s", relativePath.c_str());
  ImGui::SameLine(ImGui::GetWindowWidth() - 150);
  static bool s_ShowMetaFiles = false;
  ImGui::Checkbox("Show .meta", &s_ShowMetaFiles);

  // 返回上一级按钮（仅在不在根目录时显示）
  if (s_CurrentPath != assetsPath) {
    if (ImGui::Button("..  [Back]")) {
      std::filesystem::path p(s_CurrentPath);
      s_CurrentPath = p.parent_path().string();
      s_SelectedFile = "";
    }
    ImGui::SameLine();
  }

  // 新建文件夹按钮
  if (ImGui::Button("+ 新建文件夹")) {
    s_ShowNewFolderDialog = true;
    s_NewFolderName[0] = '\0';
  }

  // 新建文件夹弹窗逻辑
  if (s_ShowNewFolderDialog) {
    ImGui::OpenPopup("新建文件夹");
  }

  if (ImGui::BeginPopupModal("新建文件夹", &s_ShowNewFolderDialog,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("输入文件夹名称:");
    ImGui::InputText("##foldername", s_NewFolderName, sizeof(s_NewFolderName));

    if (ImGui::Button("创建", ImVec2(120, 0))) {
      CreateFolder();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("取消", ImVec2(120, 0))) {
      s_ShowNewFolderDialog = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGui::Separator();

  // 使用三栏布局：左侧信息，中间列表，右侧预览
  ImGui::Columns(3, "ContentBrowserColumns", true);
  ImGui::SetColumnWidth(0, 200.0f);
  ImGui::SetColumnWidth(1, ImGui::GetWindowWidth() - 500.0f);

  // 左侧：选中文件信息
  ImGui::BeginChild("FileInfo", ImVec2(0, 0), true);
  if (!s_SelectedFile.empty()) {
    try {
      ImGui::Text("Selected:");
      ImGui::TextWrapped("%s", s_SelectedFile.c_str());
      ImGui::Separator();

      std::filesystem::path filePath =
          std::filesystem::path(s_CurrentPath) / s_SelectedFile;
      if (std::filesystem::exists(filePath)) {
        if (std::filesystem::is_directory(filePath)) {
          ImGui::Text("Type: Folder");
        } else {
          ImGui::Text("Type: File");
          auto size = std::filesystem::file_size(filePath);
          if (size < 1024) {
            ImGui::Text("Size: %llu B", size);
          } else if (size < 1024 * 1024) {
            ImGui::Text("Size: %.1f KB", size / 1024.0f);
          } else {
            ImGui::Text("Size: %.1f MB", size / (1024.0f * 1024.0f));
          }

          // 如果是材质文件，解析并显示材质内部名称
          if (s_SelectedFile.size() > 9 &&
              s_SelectedFile.substr(s_SelectedFile.size() - 9) == ".mat.json") {
            try {
              std::ifstream f(filePath);
              nlohmann::json j = nlohmann::json::parse(f);
              if (j.contains("name")) {
                std::string matName = j["name"];
                ImGui::Separator();
                ImGui::Text("Material Name:");
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s",
                                   matName.c_str());
              }
            } catch (...) {
              // 忽略解析错误
            }
          }
        }
      }
    } catch (const std::exception &e) {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", e.what());
    }
  } else {
    ImGui::TextDisabled("无文件被选中");
  }
  ImGui::EndChild();

  // 右侧：文件列表
  ImGui::NextColumn();
  ImGui::BeginChild("FileList", ImVec2(0, 0), true);

  try {
    if (std::filesystem::exists(s_CurrentPath) &&
        std::filesystem::is_directory(s_CurrentPath)) {
      // 将目录和文件分开收集，以便优先显示目录
      std::vector<std::filesystem::directory_entry> directories;
      std::vector<std::filesystem::directory_entry> files;

      for (const auto &entry :
           std::filesystem::directory_iterator(s_CurrentPath)) {
        if (entry.is_directory()) {
          directories.push_back(entry);
        } else {
          files.push_back(entry);
        }
      }

      // 按名称字母顺序排序
      auto sortByName = [](const std::filesystem::directory_entry &a,
                           const std::filesystem::directory_entry &b) {
        try {
          return a.path().filename().u8string() <
                 b.path().filename().u8string();
        } catch (...) {
          return false;
        }
      };
      std::sort(directories.begin(), directories.end(), sortByName);
      std::sort(files.begin(), files.end(), sortByName);

      // 首先渲染目录
      for (const auto &entry : directories) {
        std::string filename;
        try {
          filename = entry.path().filename().u8string();
        } catch (...) {
          filename = "Invalid Encoding";
        }
        std::string name = "[D] " + filename;

        bool isSelected = (s_SelectedFile == filename);
        if (ImGui::Selectable(name.c_str(), isSelected)) {
          s_SelectedFile = filename;
        }

        if (ImGui::IsItemHovered() &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          s_CurrentPath = entry.path().u8string();
          s_SelectedFile = "";
        }

        // 文件夹作为拖拽目标（实现文件移动功能）
        if (ImGui::BeginDragDropTarget()) {
          if (const ImGuiPayload *payload =
                  ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
            const char *sourcePath = (const char *)payload->Data;
            std::filesystem::path src(sourcePath);
            std::filesystem::path dst = entry.path() / src.filename();
            try {
              std::filesystem::rename(src, dst);
              LOG_I("Moved file {} to {}", src.string(), dst.string());
            } catch (const std::exception &e) {
              LOG_E("Failed to move file: {}", e.what());
            }
          }
          ImGui::EndDragDropTarget();
        }
      }

      // 渲染文件
      for (const auto &entry : files) {
        std::string filename;
        try {
          filename = entry.path().filename().u8string();
        } catch (...) {
          filename = "Invalid Encoding";
        }

        // Filter .meta files
        if (!s_ShowMetaFiles) {
          std::string ext = entry.path().extension().u8string();
          std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
          if (ext == ".meta") {
            continue;
          }
        }

        bool isSelected = (s_SelectedFile == filename);

        if (ImGui::Selectable(filename.c_str(), isSelected,
                              ImGuiSelectableFlags_AllowDoubleClick)) {
          s_SelectedFile = filename;
        }

        // 拖拽源 (文件)
        if (ImGui::BeginDragDropSource()) {
          // 使用 u8string() 确保编码一致
          std::string pathStr = entry.path().u8string();
          ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", pathStr.c_str(),
                                    pathStr.length() + 1);
          ImGui::Text("%s", filename.c_str());
          ImGui::EndDragDropSource();
        }

        // Right-click context menu for files
        if (ImGui::BeginPopupContextItem(("文件上下文##" + filename).c_str())) {
          std::filesystem::path filePath =
              std::filesystem::path(s_CurrentPath) / filename;

          if (ImGui::MenuItem("重命名")) {
            s_ShowRenameDialog = true;
            s_RenameTargetFile = filename;
            strncpy(s_RenameBuffer, filename.c_str(), sizeof(s_RenameBuffer));
          }

          if (ImGui::MenuItem("删除")) {
            try {
              // 处理材质删除的特殊逻辑
              std::string ext;
              try {
                ext = filePath.extension().u8string();
              } catch (...) {
                ext = filePath.extension().string();
              }
              std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

              if (ext == ".mat.json") {
                UUID matID = AssetManager::GetInstance().GetAssetGUID(filePath);
                if (matID.IsValid()) {
                  LOG_I("Deleting material asset: {}, clearing from scenes...",
                        matID.ToString());

                  // 1. 在所有加载的场景中清除该材质的引用
                  for (auto &scene : EditorGUI::s_Scenes) {
                    if (scene && scene->GetRootNode()) {
                      ClearMaterialInScene(scene->GetRootNode(), matID);
                    }
                  }

                  // 2. 从渲染核心缓存中删除
                  RenderCore::DeleteMaterial(matID);

                  // 3. 从资产管理器注销
                  AssetManager::GetInstance().UnregisterAsset(matID);
                }
              }

              // 通用的 .meta 文件清理
              std::filesystem::path metaPath = filePath;
              metaPath += ".meta";
              if (std::filesystem::exists(metaPath)) {
                std::filesystem::remove(metaPath);
                LOG_I("Deleted meta file: {}", metaPath.u8string());
              }

              std::filesystem::remove(filePath);
              LOG_I("Deleted file: {}", filePath.u8string());
              s_SelectedFile = "";
            } catch (const std::exception &e) {
              LOG_E("Failed to delete file: {}", e.what());
            }
          }

          if (ImGui::MenuItem("剪切")) {
            s_ClipboardPath = filePath.u8string();
            LOG_I("Cut file: {}", filename);
          }

          if (ImGui::MenuItem("粘贴", nullptr, false,
                              !s_ClipboardPath.empty())) {
            try {
              std::filesystem::path srcPath(s_ClipboardPath);
              std::filesystem::path dstPath =
                  std::filesystem::path(s_CurrentPath) / srcPath.filename();

              // Find unique name if file already exists
              if (std::filesystem::exists(dstPath)) {
                std::string stem = dstPath.stem().string();
                std::string ext = dstPath.extension().string();
                int copyNum = 1;
                do {
                  std::string newName =
                      stem + "_" + std::to_string(copyNum) + ext;
                  dstPath = std::filesystem::path(s_CurrentPath) / newName;
                  copyNum++;
                } while (std::filesystem::exists(dstPath));
              }

              // Move the file (cut operation)
              std::filesystem::rename(srcPath, dstPath);
              LOG_I("Moved file {} to {}", srcPath.u8string(),
                    dstPath.u8string());
              s_ClipboardPath = ""; // Clear clipboard after paste
            } catch (const std::exception &e) {
              LOG_E("Failed to paste file: {}", e.what());
            }
          }

          if (ImGui::MenuItem("创建副本")) {
            try {
              std::filesystem::path srcPath = filePath;
              std::filesystem::path dstPath = filePath;
              std::string stem = dstPath.stem().string();
              std::string ext = dstPath.extension().string();
              std::string newName = stem + "_copy" + ext;
              dstPath.replace_filename(newName);

              // Find unique name if copy already exists
              int copyNum = 1;
              while (std::filesystem::exists(dstPath)) {
                newName = stem + "_copy" + std::to_string(copyNum) + ext;
                dstPath.replace_filename(newName);
                copyNum++;
              }

              std::filesystem::copy_file(srcPath, dstPath);
              LOG_I("Created copy: {}", dstPath.u8string());
            } catch (const std::exception &e) {
              LOG_E("Failed to create copy: {}", e.what());
            }
          }

          // 针对 .mesh 文件的特殊右键项：直接添加到当前场景
          std::string ext = filePath.extension().string();
          std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
          if (ext == ".mesh" && s_CurrentScene) {
            if (ImGui::MenuItem("添加网格到场景")) {
              try {
                // 通过资产管理器获取该 mesh 文件的 UUID
                UUID meshID =
                    AssetManager::GetInstance().GetAssetGUID(filePath);

                if (meshID.IsValid()) {
                  // 创建一个新的 MeshNode，名称默认为文件名
                  auto meshNode =
                      std::make_unique<MeshNode>(filePath.stem().string());
                  meshNode->SetMeshID(meshID);

                  // 如果当前有选中的节点，作为其子节点添加；否则直接添加到场景根部
                  Node *parent = GetSelectedNode();
                  if (parent) {
                    parent->AddChild(std::move(meshNode));
                  } else {
                    s_CurrentScene->AddNode(std::move(meshNode));
                  }

                  LOG_I("已添加网格到场景: {}", filename);
                } else {
                  LOG_E("该网格文件未在资产管理器中注册: {}", filename);
                }
              } catch (const std::exception &e) {
                LOG_E("添加网格失败: {}", e.what());
              }
            }
          }

          ImGui::EndPopup();
        }

        // 文件作为拖拽源
        if (ImGui::BeginDragDropSource()) {
          std::string fullPath = entry.path().u8string();
          ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", fullPath.c_str(),
                                    fullPath.size() + 1);
          ImGui::Text("%s", filename.c_str());
          ImGui::EndDragDropSource();
        }
      }

      if (directories.empty() && files.empty()) {
        ImGui::TextDisabled("(空文件夹)");
      }
    } else {
      ImGui::TextDisabled("路径无效");
    }
  } catch (const std::exception &e) {
    LOG_E("Failed to list files: {}", e.what());
  }

  ImGui::EndChild();

  // 第三列：预览
  ImGui::NextColumn();
  ImGui::BeginChild("Preview", ImVec2(0, 0), true);
  ImGui::Text("预览");
  ImGui::Separator();

  if (!s_SelectedFile.empty()) {
    std::filesystem::path filePath =
        std::filesystem::path(s_CurrentPath) / s_SelectedFile;
    std::string ext = filePath.extension().u8string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
        ext == ".tga") {
      UUID texID = AssetManager::GetInstance().GetAssetGUID(filePath);
      if (texID.IsValid()) {
        ImTextureID texHandle = RenderCore::GetImGuiTextureID(texID);
        if (texHandle) {
          float windowWidth = ImGui::GetContentRegionAvail().x;
          ImGui::Image(texHandle, ImVec2(windowWidth, windowWidth));
          ImGui::Text("Dimension: %dx%d",
                      RenderCore::GetTextureResource(texID)->width,
                      RenderCore::GetTextureResource(texID)->height);
        } else {
          ImGui::TextDisabled("纹理未加载或预览不可用");
        }
      } else {
        ImGui::TextDisabled("该文件未作为资产注册 (无 .meta)");
      }
    } else {
      ImGui::TextDisabled("该文件类型不支持预览");
    }
  }

  ImGui::EndChild();

  ImGui::Columns(1);

  // 渲染各种对话框
  if (s_ShowRenameDialog) {
    ImGui::OpenPopup("重命名文件");
  }

  if (ImGui::BeginPopupModal("重命名文件", &s_ShowRenameDialog,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("输入新名称:");
    ImGui::InputText("##renamebuf", s_RenameBuffer, sizeof(s_RenameBuffer));

    if (ImGui::Button("确定", ImVec2(120, 0))) {
      std::filesystem::path oldFile =
          std::filesystem::u8path(s_CurrentPath) /
          std::filesystem::u8path(s_RenameTargetFile);
      std::filesystem::path newFile = std::filesystem::u8path(s_CurrentPath) /
                                      std::filesystem::u8path(s_RenameBuffer);
      try {
        std::filesystem::rename(oldFile, newFile);

        // 如果存在 .meta 文件，也一并重命名
        std::filesystem::path oldMeta =
            std::filesystem::u8path(oldFile.u8string() + ".meta");
        std::filesystem::path newMeta =
            std::filesystem::u8path(newFile.u8string() + ".meta");
        if (std::filesystem::exists(oldMeta)) {
          std::filesystem::rename(oldMeta, newMeta);
        }

        // 刷新资产管理器
        AssetManager::GetInstance().ScanAssets();
        s_SelectedFile = s_RenameBuffer;
        LOG_I("Renamed {} to {}", s_RenameTargetFile, s_RenameBuffer);
      } catch (const std::exception &e) {
        LOG_E("Failed to rename: {}", e.what());
      }
      s_ShowRenameDialog = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("取消", ImVec2(120, 0))) {
      s_ShowRenameDialog = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGui::End();
}

bool EditorGUI::VerticalTab(const char *label, bool selected,
                            const ImVec2 &size) {
  ImGuiStyle &style = ImGui::GetStyle();

  if (selected) {
    ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
  }

  bool clicked = ImGui::Button(label, size);

  if (selected) {
    ImGui::PopStyleColor();
  }

  return clicked;
}

// ========== 创建节点函数 ==========

void EditorGUI::CreateCube() {
  auto project = RenderCore::GetCurrentProject();
  if (!project || !s_CurrentScene) {
    LOG_W("No project or scene loaded, cannot create cube");
    return;
  }

  // 使用 ModelImporter 导入内置的 Cube 几何体
  // 这会将 .obj 转换为引擎内部的 .mesh 格式并注册到 AssetManager
  ModelImporter importer;
  std::string modelPath = "resource/models/build_in/Box.obj";
  auto result = importer.ImportToScene(modelPath, project->GetAssetsPath(),
                                       *s_CurrentScene);

  if (result.success) {
    LOG_I("Created Cube and added to scene");
  } else {
    LOG_E("Failed to create Cube: {}", result.errorMessage);
  }
}

void EditorGUI::CreateSphere() {
  auto project = RenderCore::GetCurrentProject();
  if (!project || !s_CurrentScene) {
    LOG_W("No project or scene loaded, cannot create sphere");
    return;
  }

  // 使用 ModelImporter 导入内置的 Sphere 几何体
  ModelImporter importer;
  std::string modelPath = "resource/models/build_in/sphere.obj";
  auto result = importer.ImportToScene(modelPath, project->GetAssetsPath(),
                                       *s_CurrentScene);

  if (result.success) {
    LOG_I("Created Sphere and added to scene");
  } else {
    LOG_E("Failed to create Sphere: {}", result.errorMessage);
  }
}

void EditorGUI::CreatePlane() {
  auto project = RenderCore::GetCurrentProject();
  if (!project || !s_CurrentScene) {
    LOG_W("No project or scene loaded, cannot create plane");
    return;
  }

  // 使用 ModelImporter 导入内置的 Plane 几何体
  ModelImporter importer;
  std::string modelPath = "resource/models/build_in/Plane.obj";
  auto result = importer.ImportToScene(modelPath, project->GetAssetsPath(),
                                       *s_CurrentScene);

  if (result.success) {
    LOG_I("Created Plane and added to scene");
  } else {
    LOG_E("Failed to create Plane: {}", result.errorMessage);
  }
}

void EditorGUI::CreatePointLight() {
  if (!s_CurrentScene) {
    LOG_W("No scene loaded, cannot create point light");
    return;
  }

  auto pointLight = std::make_unique<PointLightNode>("Point Light");
  pointLight->SetPosition(glm::vec3(0.0f, 2.0f, 0.0f));
  pointLight->SetColor(glm::vec3(1.0f));
  pointLight->SetIntensity(1.0f);
  pointLight->SetRadius(10.0f);
  s_CurrentScene->AddNode(std::move(pointLight));
  LOG_I("Created Point Light");
}

void EditorGUI::CreateCamera() {
  if (!s_CurrentScene) {
    LOG_W("No scene loaded, cannot create camera");
    return;
  }

  auto node = std::make_unique<CameraNode>("Camera");
  s_CurrentScene->AddNode(std::move(node));
  LOG_I("Created CameraNode");
}

// ========== 工程管理函数 ==========

std::string EditorGUI::ShowFileDialog(bool isOpen, const char *filter) {
  wchar_t filename[MAX_PATH] = L"";

  // 将 filter 转换为宽字符 (注意 filter 可能有多个 \0)
  std::wstring wfilter;
  if (filter) {
    const char *p = filter;
    while (*p || *(p + 1)) {
      wfilter += (wchar_t)*p;
      p++;
    }
    wfilter += L'\0';
    wfilter += L'\0';
  }

  OPENFILENAMEW ofn;
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = NULL;
  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = wfilter.empty() ? NULL : wfilter.c_str();
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

  BOOL success = FALSE;
  if (isOpen) {
    success = GetOpenFileNameW(&ofn);
  } else {
    success = GetSaveFileNameW(&ofn);
  }

  if (success) {
    // 将宽字符路径转换为 UTF-8 字符串
    return std::filesystem::path(filename).u8string();
  }

  return "";
}

void EditorGUI::CreateNewProject() {
  // 使用文件夹选择对话框 (宽字符版本)
  wchar_t folderPath[MAX_PATH] = L"";

  BROWSEINFOW bi;
  ZeroMemory(&bi, sizeof(bi));
  bi.lpszTitle = L"Select folder for new project";
  bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

  LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
  if (pidl != NULL) {
    SHGetPathFromIDListW(pidl, folderPath);
    CoTaskMemFree(pidl);

    std::string projectPath = std::filesystem::path(folderPath).u8string();
    if (!projectPath.empty()) {
      // 创建新工程
      auto project = Project::Create(projectPath, "NewProject");
      if (project) {
        RenderCore::SetCurrentProject(project);

        // 清除现有场景
        s_Scenes.clear();
        s_SelectedNode = nullptr;
        s_CurrentPath = "";

        s_CurrentScene = nullptr;
        s_ActiveSceneIndex = -1;

        LOG_I("Created new project at: {}", projectPath);
      } else {
        LOG_E("Failed to create project");
      }
    }
  }
}

void EditorGUI::LoadProject() {
  // 选择project.json文件
  std::string projectFile =
      ShowFileDialog(true, "Project Files (*.json)\0*.json\0All Files\0*.*\0");

  if (!projectFile.empty()) {
    // 获取工程目录（project.json的父目录）
    std::string projectPath =
        std::filesystem::u8path(projectFile).parent_path().u8string();

    auto project = Project::Load(projectPath);
    if (project) {
      RenderCore::SetCurrentProject(project);

      // 清除现有场景
      s_Scenes.clear();
      s_CurrentScene = nullptr;
      s_ActiveSceneIndex = -1;
      s_SelectedNode = nullptr;

      // 加载所有场景
      for (const auto &scenePath : project->GetScenePaths()) {
        auto scene = Scene::Load(scenePath);
        if (scene) {
          s_Scenes.push_back(scene);
          LOG_I("Loaded scene: {}", scenePath);
        }
      }

      // 设置激活场景
      std::string activeScenePath = project->GetActiveScenePath();
      if (!activeScenePath.empty()) {
        for (size_t i = 0; i < s_Scenes.size(); i++) {
          // Check if this is the active scene
          std::string scenePath = project->GetProjectPath() + "/Scenes/" +
                                  s_Scenes[i]->GetName() + ".json";
          if (scenePath == activeScenePath) {
            s_ActiveSceneIndex = static_cast<int>(i);
            s_CurrentScene = s_Scenes[i];
            break;
          }
        }
      }

      if (s_Scenes.empty()) {
        LOG_I("No scenes found, project is empty");
      } else if (!s_CurrentScene && !s_Scenes.empty()) {
        s_ActiveSceneIndex = 0;
        s_CurrentScene = s_Scenes[0];
      }

      // 恢复视角设置
      const auto &guiSettings = project->GetGuiSettings();
      if (guiSettings.contains("viewCam")) {
        auto &camera = RenderCore::GetCamera();
        const auto &camJson = guiSettings["viewCam"];
        camera.SetPosition(
            glm::vec3(camJson["pos"][0], camJson["pos"][1], camJson["pos"][2]));
        camera.ProcessMouseMovement(
            camJson.value("yaw", -90.0f) - camera.GetYaw(),
            camJson.value("pitch", 0.0f) - camera.GetPitch(), false);
        camera.SetFov(camJson.value("fov", 45.0f));
        camera.SetMovementSpeed(camJson.value("speed", 2.5f));
        camera.SetMouseSensitivity(camJson.value("sensitivity", 0.1f));
        camera.SetNearPlane(camJson.value("near", 0.1f));
        camera.SetFarPlane(camJson.value("far", 100.0f));
      }

      LOG_I("Loaded project from: {}", projectPath);
    } else {
      LOG_E("Failed to load project");
    }
  }
}

void EditorGUI::SaveProject() {
  auto project = RenderCore::GetCurrentProject();
  if (!project) {
    LOG_W("No project to save");
    return;
  }

  // 保存所有场景
  std::string scenesDir = project->GetProjectPath() + "/Scenes";
  std::filesystem::create_directories(scenesDir);

  // 清除旧的场景列表，准备重新构建
  project->ClearScenes();
  std::set<std::string> currentSceneFiles;

  for (const auto &scene : s_Scenes) {
    std::string scenePath = scenesDir + "/" + scene->GetName() + ".json";
    if (scene->Save(scenePath)) {
      project->AddScene(scenePath);
      currentSceneFiles.insert(scene->GetName() + ".json");
      LOG_I("Saved scene: {}", scenePath);
    } else {
      LOG_E("Failed to save scene: {}", scene->GetName());
    }
  }

  // 删除不再存在的场景文件
  try {
    if (std::filesystem::exists(scenesDir)) {
      for (const auto &entry : std::filesystem::directory_iterator(scenesDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
          std::string filename = entry.path().filename().string();
          if (currentSceneFiles.find(filename) == currentSceneFiles.end()) {
            std::filesystem::remove(entry.path());
            LOG_I("Deleted orphaned scene file: {}", filename);
          }
        }
      }
    }
  } catch (const std::exception &e) {
    LOG_E("Failed to clean up scene files: {}", e.what());
  }

  // 设置当前激活场景
  if (s_CurrentScene) {
    std::string activeScenePath =
        scenesDir + "/" + s_CurrentScene->GetName() + ".json";
    project->SetActiveScenePath(activeScenePath);
  } else {
    project->SetActiveScenePath("");
  }

  // 保存视角设置
  auto &camera = RenderCore::GetCamera();
  nlohmann::json guiSettings;
  guiSettings["viewCam"] = {{"pos",
                             {camera.GetPosition().x, camera.GetPosition().y,
                              camera.GetPosition().z}},
                            {"yaw", camera.GetYaw()},
                            {"pitch", camera.GetPitch()},
                            {"fov", camera.GetFov()},
                            {"speed", camera.GetMovementSpeed()},
                            {"sensitivity", camera.GetMouseSensitivity()},
                            {"near", camera.GetNearPlane()},
                            {"far", camera.GetFarPlane()}};
  project->SetGuiSettings(guiSettings);

  // 保存工程文件
  // 保存所有材质资源
  RenderCore::SaveAllMaterials();

  if (project->Save()) {
    LOG_I("Project saved successfully");
  } else {
    LOG_E("Failed to save project");
  }
}

// ========== 场景和节点创建函数 ==========

void EditorGUI::CreateScene() {
  auto project = RenderCore::GetCurrentProject();
  if (!project) {
    LOG_W("No project loaded, cannot create scene");
    return;
  }

  // Generate unique scene name
  std::string baseName = "Scene";
  std::string sceneName = baseName;
  int counter = 1;

  while (true) {
    bool exists = false;
    for (const auto &scene : s_Scenes) {
      if (scene->GetName() == sceneName) {
        exists = true;
        break;
      }
    }
    if (!exists)
      break;
    sceneName = baseName + std::to_string(counter++);
  }

  auto scene = Scene::Create(sceneName);
  s_Scenes.push_back(scene);
  s_ActiveSceneIndex = static_cast<int>(s_Scenes.size()) - 1;
  s_CurrentScene = scene;
  s_SelectedNode = nullptr;

  LOG_I("Created scene: {}", sceneName);
}

void EditorGUI::CreateEmptyNode() {
  if (!s_CurrentScene) {
    LOG_W("No scene loaded, cannot create node");
    return;
  }

  auto node = std::make_unique<Node>("Empty Node");
  s_CurrentScene->AddNode(std::move(node));
  LOG_I("Created empty node");
}

void EditorGUI::DeleteSelectedNode() {
  if (!s_SelectedNode || !s_CurrentScene) {
    LOG_W("No node selected to delete");
    return;
  }

  // Get parent node and remove the selected child
  Node *parent = s_SelectedNode->GetParent();
  if (parent) {
    s_PendingDeleteNode = s_SelectedNode;
    LOG_I("Pending delete node: {}", s_SelectedNode->GetName());
  } else {
    LOG_W("Cannot delete root node");
  }
}

void EditorGUI::CreateFolder() {
  if (s_NewFolderName[0] == '\0') {
    LOG_W("Folder name is empty");
    return;
  }

  std::filesystem::path newPath =
      std::filesystem::path(s_CurrentPath) / s_NewFolderName;

  try {
    std::filesystem::create_directory(newPath);
    LOG_I("Created folder: {}", newPath.string());
    s_NewFolderName[0] = '\0';
    s_ShowNewFolderDialog = false;
  } catch (const std::exception &e) {
    LOG_E("Failed to create folder: {}", e.what());
  }
}

} // namespace neurender
