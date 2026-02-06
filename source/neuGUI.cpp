#include "neuGUI.h"
#include "Nodes/Node.h"
#include "Project/Project.h"
#include "RenderCore.h"
#include "Scene/Scene.h"
#include "Window.h"
#include "neuLog.h"
#include <algorithm>
#include <filesystem>
#include <imgui.h>
#include <imgui_internal.h>
#include <shlobj.h>
#include <windows.h>

namespace neurender {

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
EditorGUI::RenderMode EditorGUI::s_RenderMode = RenderMode::Shaded;
bool EditorGUI::s_DockSpaceInitialized = false;

void EditorGUI::Initialize() {
  LOG_I("EditorGUI Initialized");

  // 不再创建默认场景，等待用户加载或创建工程
  // s_CurrentScene = Scene::Create("DefaultScene");
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
    MenuDelete();
    MenuDebug();

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
  if (ImGui::BeginMenu("File")) {
    // 工程管理
    if (ImGui::MenuItem("New Project...", "Ctrl+Shift+N")) {
      CreateNewProject();
    }
    if (ImGui::MenuItem("Load Project...", "Ctrl+Shift+O")) {
      LoadProject();
    }

    auto currentProject = RenderCore::GetCurrentProject();
    bool hasProject = (currentProject != nullptr);

    if (ImGui::MenuItem("Save Project", "Ctrl+Shift+S", false, hasProject)) {
      SaveProject();
    }

    ImGui::Separator();

    // 导入资源
    if (ImGui::MenuItem("Import Model...", nullptr, false, hasProject)) {
      std::string modelFile = ShowFileDialog(
          true, "Model Files (*.obj;*.gltf;*.glb)\0*.obj;*.gltf;*.glb\0All "
                "Files\0*.*\0");
      if (!modelFile.empty()) {
        // Copy to Assets directory
        std::filesystem::path src(modelFile);
        std::filesystem::path dst =
            std::filesystem::path(currentProject->GetAssetsPath()) /
            src.filename();
        try {
          std::filesystem::copy_file(
              src, dst, std::filesystem::copy_options::overwrite_existing);
          LOG_I("Imported model: {}", dst.string());
        } catch (const std::exception &e) {
          LOG_E("Failed to import model: {}", e.what());
        }
      }
    }

    if (ImGui::MenuItem("Import Texture...", nullptr, false, hasProject)) {
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
          LOG_I("Imported texture: {}", dst.string());
        } catch (const std::exception &e) {
          LOG_E("Failed to import texture: {}", e.what());
        }
      }
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Exit", "Alt+F4")) {
      Window::Close();
    }

    ImGui::EndMenu();
  }
}

void EditorGUI::MenuCreate() {
  auto project = RenderCore::GetCurrentProject();
  bool hasProject = (project != nullptr);

  if (ImGui::BeginMenu("Create")) {
    // 场景
    if (ImGui::MenuItem("Scene", nullptr, false, hasProject)) {
      CreateScene();
    }

    // 空物体
    if (ImGui::MenuItem("Empty Node", nullptr, false,
                        s_CurrentScene != nullptr)) {
      CreateEmptyNode();
    }

    ImGui::Separator();

    // 3D Objects
    if (ImGui::BeginMenu("3D Object", s_CurrentScene != nullptr)) {
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
    if (ImGui::BeginMenu("Light", s_CurrentScene != nullptr)) {
      if (ImGui::MenuItem("Point Light")) {
        CreatePointLight();
      }
      if (ImGui::MenuItem("Directional Light")) {
        CreateDirectionalLight();
      }
      ImGui::EndMenu();
    }

    // Camera
    if (ImGui::MenuItem("Camera", nullptr, false, s_CurrentScene != nullptr)) {
      CreateCamera();
    }

    ImGui::EndMenu();
  }
}

void EditorGUI::MenuDelete() {
  if (ImGui::BeginMenu("Delete")) {
    bool canDelete = (s_SelectedNode != nullptr && s_CurrentScene != nullptr);

    if (ImGui::MenuItem("Delete Selected Node", "Delete", false, canDelete)) {
      DeleteSelectedNode();
    }

    ImGui::EndMenu();
  }
}

void EditorGUI::MenuDebug() {
  if (ImGui::BeginMenu("Debug")) {
    if (ImGui::MenuItem("Shaded", nullptr,
                        s_RenderMode == RenderMode::Shaded)) {
      s_RenderMode = RenderMode::Shaded;
      LOG_I("Render mode: Shaded");
    }
    if (ImGui::MenuItem("Wireframe", nullptr,
                        s_RenderMode == RenderMode::Wireframe)) {
      s_RenderMode = RenderMode::Wireframe;
      LOG_I("Render mode: Wireframe");
    }
    if (ImGui::MenuItem("Albedo", nullptr,
                        s_RenderMode == RenderMode::Albedo)) {
      s_RenderMode = RenderMode::Albedo;
      LOG_I("Render mode: Albedo");
    }
    if (ImGui::MenuItem("Normal", nullptr,
                        s_RenderMode == RenderMode::Normal)) {
      s_RenderMode = RenderMode::Normal;
      LOG_I("Render mode: Normal");
    }
    if (ImGui::MenuItem("Depth", nullptr, s_RenderMode == RenderMode::Depth)) {
      s_RenderMode = RenderMode::Depth;
      LOG_I("Render mode: Depth");
    }

    ImGui::EndMenu();
  }
}

void EditorGUI::RenderSceneHierarchy() {
  ImGui::Begin("Scene Hierarchy");

  auto project = RenderCore::GetCurrentProject();
  if (!project) {
    ImGui::TextDisabled("No project loaded");
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
      bool sceneOpen = ImGui::TreeNodeEx(sceneLabel.c_str(), sceneFlags);

      // Click to select scene
      if (ImGui::IsItemClicked()) {
        s_ActiveSceneIndex = static_cast<int>(i);
        s_CurrentScene = scene;
        s_SelectedNode = nullptr;
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
      ImGui::TextDisabled("No scenes - Create one in Create menu");
    }

    ImGui::TreePop();
  }

  // Right-click context menu
  if (ImGui::BeginPopupContextWindow("SceneHierarchyContext")) {
    if (ImGui::MenuItem("Create Scene")) {
      CreateScene();
    }
    if (s_CurrentScene) {
      if (ImGui::MenuItem("Create Empty Node")) {
        CreateEmptyNode();
      }
    }
    ImGui::EndPopup();
  }

  ImGui::End();
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
  if (node->GetChildren().empty()) {
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
        // TODO: 重新设置父子关系
        LOG_I("Reparent {0} to {1}", draggedNode->GetName(), node->GetName());
      }
    }
    ImGui::EndDragDropTarget();
  }

  // 右键菜单
  if (ImGui::BeginPopupContextItem()) {
    if (ImGui::MenuItem("Delete")) {
      // TODO: 删除节点
      LOG_I("Delete node {0}", node->GetName());
    }
    if (ImGui::MenuItem("Duplicate")) {
      // TODO: 复制节点
      LOG_I("Duplicate node {0}", node->GetName());
    }
    ImGui::EndPopup();
  }

  // 递归渲染子节点
  if (nodeOpen && !node->GetChildren().empty()) {
    for (const auto &child : node->GetChildren()) {
      RenderNodeTree(child.get());
    }
    ImGui::TreePop();
  }
}

void EditorGUI::RenderInspector() {
  ImGui::Begin("Inspector");

  if (!s_SelectedNode) {
    ImGui::TextDisabled("No object selected");
    ImGui::End();
    return;
  }

  // 顶部：对象名称和UUID
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
  ImGui::BeginChild("InspectorHeader", ImVec2(0, 80), true);

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

  // Transform Tab
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
  }

  ImGui::EndChild();

  // 右侧：属性编辑区
  ImGui::NextColumn();
  ImGui::BeginChild("Properties", ImVec2(0, 0), true);

  switch (s_CurrentInspectorTab) {
  case 0: // World
    ImGui::Text("World Environment Settings");
    ImGui::Separator();
    // TODO: 世界环境设置
    ImGui::Text("Ambient Light");
    ImGui::Text("Skybox");
    ImGui::Text("Fog");
    break;

  case 1: // Transform
    RenderTransformEditor(s_SelectedNode);
    break;

  case 2: // Mesh
    if (nodeType == "MeshNode") {
      RenderMeshNodeInspector(s_SelectedNode);
    }
    break;

  case 3: // Material
    if (nodeType == "MeshNode") {
      ImGui::Text("Material Properties");
      ImGui::Separator();
      // TODO: 材质属性编辑
      ImGui::Text("Base Color");
      ImGui::Text("Metallic");
      ImGui::Text("Roughness");
    }
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

void EditorGUI::RenderContentBrowser() {
  ImGui::Begin("Content Browser");

  auto project = RenderCore::GetCurrentProject();
  if (!project) {
    ImGui::TextDisabled("No project loaded");
    ImGui::End();
    return;
  }

  std::string assetsPath = project->GetAssetsPath();

  // Initialize current path to assets path if empty
  if (s_CurrentPath.empty() ||
      s_CurrentPath.find(assetsPath) == std::string::npos) {
    s_CurrentPath = assetsPath;
  }

  // Show current path relative to Assets
  std::string relativePath = s_CurrentPath;
  if (s_CurrentPath.length() > assetsPath.length()) {
    relativePath = "Assets" + s_CurrentPath.substr(assetsPath.length());
  } else {
    relativePath = "Assets";
  }
  ImGui::Text("Path: %s", relativePath.c_str());

  // Back button (only if not at Assets root)
  if (s_CurrentPath != assetsPath) {
    if (ImGui::Button("..  [Back]")) {
      std::filesystem::path p(s_CurrentPath);
      s_CurrentPath = p.parent_path().string();
      s_SelectedFile = "";
    }
    ImGui::SameLine();
  }

  // New Folder button
  if (ImGui::Button("+ New Folder")) {
    s_ShowNewFolderDialog = true;
    s_NewFolderName[0] = '\0';
  }

  // New Folder dialog
  if (s_ShowNewFolderDialog) {
    ImGui::OpenPopup("New Folder");
  }

  if (ImGui::BeginPopupModal("New Folder", &s_ShowNewFolderDialog,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Enter folder name:");
    ImGui::InputText("##foldername", s_NewFolderName, sizeof(s_NewFolderName));

    if (ImGui::Button("Create", ImVec2(120, 0))) {
      CreateFolder();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      s_ShowNewFolderDialog = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGui::Separator();

  ImGui::Columns(2, "ContentBrowserColumns", true);
  ImGui::SetColumnWidth(0, 200.0f);

  // Left side: selected file info
  ImGui::BeginChild("FileInfo", ImVec2(0, 0), true);
  if (!s_SelectedFile.empty()) {
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
      }
    }
  } else {
    ImGui::TextDisabled("No file selected");
  }
  ImGui::EndChild();

  // Right side: file list
  ImGui::NextColumn();
  ImGui::BeginChild("FileList", ImVec2(0, 0), true);

  try {
    if (std::filesystem::exists(s_CurrentPath) &&
        std::filesystem::is_directory(s_CurrentPath)) {
      // Collect directories and files separately
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

      // Sort alphabetically
      auto sortByName = [](const std::filesystem::directory_entry &a,
                           const std::filesystem::directory_entry &b) {
        return a.path().filename().string() < b.path().filename().string();
      };
      std::sort(directories.begin(), directories.end(), sortByName);
      std::sort(files.begin(), files.end(), sortByName);

      // Show directories first
      for (const auto &entry : directories) {
        std::string name = "[D] " + entry.path().filename().string();
        std::string filename = entry.path().filename().string();

        if (ImGui::Selectable(name.c_str(), s_SelectedFile == filename,
                              ImGuiSelectableFlags_AllowDoubleClick)) {
          s_SelectedFile = filename;

          // Double-click to enter directory
          if (ImGui::IsMouseDoubleClicked(0)) {
            s_CurrentPath = entry.path().string();
            s_SelectedFile = "";
          }
        }
      }

      // Show files
      for (const auto &entry : files) {
        std::string filename = entry.path().filename().string();

        if (ImGui::Selectable(filename.c_str(), s_SelectedFile == filename)) {
          s_SelectedFile = filename;
        }

        // Drag source for files
        if (ImGui::BeginDragDropSource()) {
          std::string fullPath = entry.path().string();
          ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", fullPath.c_str(),
                                    fullPath.size() + 1);
          ImGui::Text("%s", filename.c_str());
          ImGui::EndDragDropSource();
        }
      }

      if (directories.empty() && files.empty()) {
        ImGui::TextDisabled("(Empty folder)");
      }
    } else {
      ImGui::TextDisabled("Invalid path");
    }
  } catch (const std::exception &e) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", e.what());
  }

  ImGui::EndChild();
  ImGui::Columns(1);

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
  if (!s_CurrentScene) {
    LOG_W("No scene loaded, cannot create cube");
    return;
  }

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  if (RenderCore::LoadModelFromFile("resource/models/build_in/Box.obj",
                                    vertices, indices)) {
    // TODO: 创建MeshNode并设置几何体数据
    auto node = std::make_unique<Node>("Cube");
    s_CurrentScene->AddNode(std::move(node));
    LOG_I("Created Cube with {} vertices", vertices.size());
  } else {
    LOG_E("Failed to load Box.obj");
  }
}

void EditorGUI::CreateSphere() {
  if (!s_CurrentScene) {
    LOG_W("No scene loaded, cannot create sphere");
    return;
  }

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  if (RenderCore::LoadModelFromFile("resource/models/build_in/sphere.obj",
                                    vertices, indices)) {
    // TODO: 创建MeshNode并设置几何体数据
    auto node = std::make_unique<Node>("Sphere");
    s_CurrentScene->AddNode(std::move(node));
    LOG_I("Created Sphere with {} vertices", vertices.size());
  } else {
    LOG_E("Failed to load sphere.obj");
  }
}

void EditorGUI::CreatePlane() {
  if (!s_CurrentScene) {
    LOG_W("No scene loaded, cannot create plane");
    return;
  }

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  if (RenderCore::LoadModelFromFile("resource/models/build_in/Plane.obj",
                                    vertices, indices)) {
    // TODO: 创建MeshNode并设置几何体数据
    auto node = std::make_unique<Node>("Plane");
    s_CurrentScene->AddNode(std::move(node));
    LOG_I("Created Plane with {} vertices", vertices.size());
  } else {
    LOG_E("Failed to load Plane.obj");
  }
}

void EditorGUI::CreatePointLight() {
  auto node = std::make_unique<Node>("Point Light");
  s_CurrentScene->AddNode(std::move(node));
  LOG_I("Created Point Light");
}

void EditorGUI::CreateDirectionalLight() {
  auto node = std::make_unique<Node>("Directional Light");
  s_CurrentScene->AddNode(std::move(node));
  LOG_I("Created Directional Light");
}

void EditorGUI::CreateCamera() {
  auto node = std::make_unique<Node>("Camera");
  s_CurrentScene->AddNode(std::move(node));
  LOG_I("Created Camera");
}

// ========== 工程管理函数 ==========

std::string EditorGUI::ShowFileDialog(bool isOpen, const char *filter) {
  char filename[MAX_PATH] = "";

  OPENFILENAMEA ofn;
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = NULL;
  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = filter;
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

  if (isOpen) {
    if (GetOpenFileNameA(&ofn)) {
      return std::string(filename);
    }
  } else {
    if (GetSaveFileNameA(&ofn)) {
      return std::string(filename);
    }
  }

  return "";
}

void EditorGUI::CreateNewProject() {
  // 使用文件夹选择对话框
  char folderPath[MAX_PATH] = "";

  BROWSEINFOA bi;
  ZeroMemory(&bi, sizeof(bi));
  bi.lpszTitle = "Select folder for new project";
  bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

  LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
  if (pidl != NULL) {
    SHGetPathFromIDListA(pidl, folderPath);
    CoTaskMemFree(pidl);

    std::string projectPath = std::string(folderPath);
    if (!projectPath.empty()) {
      // 创建新工程
      auto project = Project::Create(projectPath, "NewProject");
      if (project) {
        RenderCore::SetCurrentProject(project);

        // 清除现有场景
        s_Scenes.clear();
        s_SelectedNode = nullptr;
        s_CurrentPath = "";

        // 不创建默认场景 - 用户需要手动创建
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
    size_t lastSlash = projectFile.find_last_of("\\/");
    std::string projectPath = projectFile.substr(0, lastSlash);

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

      // 如果没有加载到场景，或没有激活场景，选择第一个
      if (s_Scenes.empty()) {
        LOG_I("No scenes found, project is empty");
      } else if (!s_CurrentScene && !s_Scenes.empty()) {
        s_ActiveSceneIndex = 0;
        s_CurrentScene = s_Scenes[0];
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

  for (const auto &scene : s_Scenes) {
    std::string scenePath = scenesDir + "/" + scene->GetName() + ".json";
    if (scene->Save(scenePath)) {
      project->AddScene(scenePath);
      LOG_I("Saved scene: {}", scenePath);
    } else {
      LOG_E("Failed to save scene: {}", scene->GetName());
    }
  }

  // 设置当前激活场景
  if (s_CurrentScene) {
    std::string activeScenePath =
        scenesDir + "/" + s_CurrentScene->GetName() + ".json";
    project->SetActiveScenePath(activeScenePath);
  }

  // 保存工程文件
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
    std::string nodeName = s_SelectedNode->GetName();
    parent->RemoveChild(s_SelectedNode);
    s_SelectedNode = nullptr;
    LOG_I("Deleted node: {}", nodeName);
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
