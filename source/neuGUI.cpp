#include "neuGUI.h"
#include "Camera.h"
#include "Nodes/MeshNode.h"
#include "Nodes/Node.h"
#include "RenderCore.h"
#include "Scene/Scene.h"
#include "Window.h"
#include "neuLog.h"
#include <imgui.h>
#include <imgui_internal.h>

namespace neurender {

// 静态成员初始化
std::shared_ptr<Scene> EditorGUI::s_CurrentScene = nullptr;
Node *EditorGUI::s_SelectedNode = nullptr;
int EditorGUI::s_CurrentInspectorTab = 0;
std::string EditorGUI::s_CurrentPath = "Assets/";
std::string EditorGUI::s_SelectedFile = "";
EditorGUI::RenderMode EditorGUI::s_RenderMode = RenderMode::Shaded;
bool EditorGUI::s_DockSpaceInitialized = false;

void EditorGUI::Initialize() {
  LOG_I("EditorGUI Initialized");

  // 创建默认场景
  s_CurrentScene = Scene::Create("DefaultScene");
}

void EditorGUI::Shutdown() { LOG_I("EditorGUI Shutdown"); }

void EditorGUI::SetCurrentScene(std::shared_ptr<Scene> scene) {
  s_CurrentScene = scene;
  s_SelectedNode = nullptr; // 清除选择
}

void EditorGUI::Render() {
  if (!s_CurrentScene) {
    return;
  }

  // 设置DockSpace
  SetupDockSpace();

  // 渲染各个面板
  RenderMenuBar();
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
    if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
      s_CurrentScene = Scene::Create("NewScene");
      s_SelectedNode = nullptr;
      LOG_I("Created new scene");
    }

    if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
      // TODO: 打开文件对话框
      LOG_I("Open scene dialog (TODO)");
    }

    if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
      if (s_CurrentScene) {
        std::string path = "Scenes/" + s_CurrentScene->GetName() + ".json";
        if (s_CurrentScene->Save(path)) {
          LOG_I("Scene saved to {0}", path);
        } else {
          LOG_E("Failed to save scene");
        }
      }
    }

    if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) {
      // TODO: 保存文件对话框
      LOG_I("Save scene as dialog (TODO)");
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Import Model...")) {
      // TODO: 导入模型对话框
      LOG_I("Import model dialog (TODO)");
    }

    if (ImGui::MenuItem("Import Texture...")) {
      // TODO: 导入纹理对话框
      LOG_I("Import texture dialog (TODO)");
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Exit", "Alt+F4")) {
      Window::Close();
    }

    ImGui::EndMenu();
  }
}

void EditorGUI::MenuCreate() {
  if (ImGui::BeginMenu("Create")) {
    if (ImGui::BeginMenu("3D Object")) {
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

    if (ImGui::BeginMenu("Light")) {
      if (ImGui::MenuItem("Point Light")) {
        CreatePointLight();
      }
      if (ImGui::MenuItem("Directional Light")) {
        CreateDirectionalLight();
      }
      ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Camera")) {
      CreateCamera();
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

    ImGui::Separator();

    if (ImGui::MenuItem("Show Stats")) {
      LOG_I("Show stats (TODO)");
    }

    ImGui::EndMenu();
  }
}

void EditorGUI::RenderSceneHierarchy() {
  ImGui::Begin("Scene Hierarchy");

  if (!s_CurrentScene) {
    ImGui::Text("No scene loaded");
    ImGui::End();
    return;
  }

  // 场景名称
  ImGui::Text("Scene: %s", s_CurrentScene->GetName().c_str());
  ImGui::Separator();

  // 渲染节点树
  Node *rootNode = s_CurrentScene->GetRootNode();
  if (rootNode) {
    for (const auto &child : rootNode->GetChildren()) {
      RenderNodeTree(child.get());
    }
  }

  // 右键菜单 - 创建节点
  if (ImGui::BeginPopupContextWindow("SceneHierarchyContext")) {
    if (ImGui::MenuItem("Create Empty Node")) {
      auto node = std::make_unique<Node>("Empty Node");
      s_CurrentScene->AddNode(std::move(node));
      LOG_I("Created empty node");
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

  ImGui::Text("Current Path: %s", s_CurrentPath.c_str());
  ImGui::Separator();

  ImGui::Columns(2, "ContentBrowserColumns", true);
  ImGui::SetColumnWidth(0, 200.0f);

  // 左侧：选中文件信息
  ImGui::BeginChild("FileInfo", ImVec2(0, 0), true);
  if (!s_SelectedFile.empty()) {
    ImGui::Text("Selected File:");
    ImGui::Text("%s", s_SelectedFile.c_str());
    ImGui::Separator();
    ImGui::Text("Size: (TODO)");
    ImGui::Text("Type: (TODO)");
  } else {
    ImGui::TextDisabled("No file selected");
  }
  ImGui::EndChild();

  // 右侧：文件列表
  ImGui::NextColumn();
  ImGui::BeginChild("FileList", ImVec2(0, 0), true);

  // TODO: 实际文件系统浏览
  ImGui::Text("File Browser (TODO)");
  ImGui::Separator();

  // 示例文件
  const char *files[] = {"model.gltf", "texture.png", "material.json"};
  for (int i = 0; i < 3; i++) {
    if (ImGui::Selectable(files[i], s_SelectedFile == files[i])) {
      s_SelectedFile = files[i];
    }

    // 拖拽源
    if (ImGui::BeginDragDropSource()) {
      ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", files[i],
                                strlen(files[i]) + 1);
      ImGui::Text("%s", files[i]);
      ImGui::EndDragDropSource();
    }
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
  auto node = std::make_unique<Node>("Cube");
  s_CurrentScene->AddNode(std::move(node));
  LOG_I("Created Cube");
}

void EditorGUI::CreateSphere() {
  auto node = std::make_unique<Node>("Sphere");
  s_CurrentScene->AddNode(std::move(node));
  LOG_I("Created Sphere");
}

void EditorGUI::CreatePlane() {
  auto node = std::make_unique<Node>("Plane");
  s_CurrentScene->AddNode(std::move(node));
  LOG_I("Created Plane");
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

} // namespace neurender
