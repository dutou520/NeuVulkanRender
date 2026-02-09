#include "neuGUI.h"
#include "Asset/AssetManager.h"
#include "Asset/ModelImporter.h"
#include "Nodes/MeshNode.h"
#include "Nodes/Node.h"
#include "Nodes/PointLightNode.h"
#include "Project/Project.h"
#include "RenderCore.h"
#include "Scene/Scene.h"
#include "neuLog.h"
#include <algorithm>
#include <filesystem>
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
EditorGUI::RenderMode EditorGUI::s_RenderMode = RenderMode::Shaded;
bool EditorGUI::s_DockSpaceInitialized = false;
std::string EditorGUI::s_ClipboardPath = "";

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
          LOG_I("Imported texture: {}", dst.string());
        } catch (const std::exception &e) {
          LOG_E("Failed to import texture: {}", e.what());
        }
      }
    }

    ImGui::Separator();

    if (ImGui::MenuItem("关闭项目")) {
      // 关闭当前项目
      RenderCore::SetCurrentProject(nullptr);
      s_Scenes.clear();
      s_CurrentScene = nullptr;
      s_ActiveSceneIndex = -1;
      s_SelectedNode = nullptr;
      s_CurrentPath = "";
      s_SelectedFile = "";
      LOG_I("Project closed");
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
      if (ImGui::MenuItem("方向光")) {
        CreateDirectionalLight();
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

void EditorGUI::RenderSceneHierarchy() {
  ImGui::Begin("场景层级");

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
      bool sceneOpen = ImGui::TreeNodeEx(sceneLabel.c_str(), sceneFlags);

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
    if (ImGui::MenuItem("删除")) {
      DeleteSelectedNode();
    }
    if (ImGui::MenuItem("复制")) {
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
  ImGui::Begin("详细信息");

  if (!s_SelectedNode) {
    ImGui::TextDisabled("无物体被选中");
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

  // PostProcess Tab（常驻）
  if (VerticalTab("PostProcess", s_CurrentInspectorTab == 5,
                  ImVec2(100.0f, 40.0f))) {
    s_CurrentInspectorTab = 5;
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
  } else if (nodeType == "PointLightNode" || nodeType == "LightNode") {
    if (VerticalTab("Light", s_CurrentInspectorTab == 4,
                    ImVec2(100.0f, 40.0f))) {
      s_CurrentInspectorTab = 4;
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
          if (ImGui::Selectable(m->GetName().c_str())) {
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
        }

        ImGui::Separator();

        // PBR Properties
        // Base Color
        if (ImGui::CollapsingHeader("基础颜色 (Base Color)",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
          // 颜色编辑控件：允许用户调整材质的颜色因子 (RGBA)
          if (ImGui::ColorEdit4("颜色因子",
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
          ImGui::Text("渲染类型: %s",
                      (matRes->material.type == MaterialType::Transparent)
                          ? "透明 (Forward)"
                          : "不透明 (Deferred)");

          // 显示基础颜色贴图的绑定状态
          ImGui::Text("贴图状态: %s", (matRes->material.textureFlags & 1)
                                          ? "已加载"
                                          : "未加载 (使用默认值)");

          // 贴图槽位：支持从资源浏览器 (Content Browser)
          // 拖拽贴图至此按钮进行分配
          ImGui::Button("基础颜色贴图槽位", ImVec2(-1, 20));
          if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload *payload =
                    ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
              const char *path = (const char *)payload->Data;
              // 通过文件路径获取资产的唯一标识符 UUID
              UUID texID =
                  AssetManager::GetInstance().GetAssetGUID(std::string(path));
              if (texID.IsValid()) {
                // 将贴图设置到材质的 0 号槽位 (基础颜色)
                RenderCore::SetMaterialTexture(materialID, 0, texID);
              }
            }
            ImGui::EndDragDropTarget();
          }
        }

        // Metallic / Roughness
        if (ImGui::CollapsingHeader("Metallic / Roughness",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
          ImGui::SliderFloat("Metallic", &matRes->material.metallicFactor, 0.0f,
                             1.0f);
          ImGui::SliderFloat("Roughness", &matRes->material.roughnessFactor,
                             0.0f, 1.0f);

          ImGui::Text("Texture: %s", (matRes->material.textureFlags & 2)
                                         ? "Loaded (G=Roughness, B=Metallic)"
                                         : "None/Default");

          ImGui::Button("Metallic/Roughness Map Slot", ImVec2(-1, 20));
          if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload *payload =
                    ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
              const char *path = (const char *)payload->Data;
              UUID texID =
                  AssetManager::GetInstance().GetAssetGUID(std::string(path));
              if (texID.IsValid()) {
                RenderCore::SetMaterialTexture(materialID, 1, texID);
              }
            }
            ImGui::EndDragDropTarget();
          }
        }

        // Normal Map
        if (ImGui::CollapsingHeader("Normal Map")) {
          ImGui::SliderFloat("Scale", &matRes->material.normalScale, 0.0f,
                             2.0f);
          ImGui::Text("Texture: %s", (matRes->material.textureFlags & 4)
                                         ? "Loaded"
                                         : "None/Default");

          ImGui::Button("Normal Map Slot", ImVec2(-1, 20));
          if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload *payload =
                    ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
              const char *path = (const char *)payload->Data;
              UUID texID =
                  AssetManager::GetInstance().GetAssetGUID(std::string(path));
              if (texID.IsValid()) {
                RenderCore::SetMaterialTexture(materialID, 2, texID);
              }
            }
            ImGui::EndDragDropTarget();
          }
        }

        // Emission
        if (ImGui::CollapsingHeader("Emission")) {
          ImGui::DragFloat("Intensity", &matRes->material.emissiveIntensity,
                           0.1f, 0.0f, 100.0f);
          ImGui::Text("Texture: %s", (matRes->material.textureFlags & 8)
                                         ? "Loaded"
                                         : "None/Default");

          ImGui::Button("Emissive Map Slot", ImVec2(-1, 20));
          if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload *payload =
                    ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
              const char *path = (const char *)payload->Data;
              UUID texID =
                  AssetManager::GetInstance().GetAssetGUID(std::string(path));
              if (texID.IsValid()) {
                RenderCore::SetMaterialTexture(materialID, 3, texID);
              }
            }
            ImGui::EndDragDropTarget();
          }
        }

        // Occlusion
        if (ImGui::CollapsingHeader("Occlusion")) {
          ImGui::Text("Texture: %s", (matRes->material.textureFlags & 16)
                                         ? "Loaded"
                                         : "None/Default");
          ImGui::Button("Occlusion Map Slot", ImVec2(-1, 20));
          if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload *payload =
                    ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
              const char *path = (const char *)payload->Data;
              UUID texID =
                  AssetManager::GetInstance().GetAssetGUID(std::string(path));
              if (texID.IsValid()) {
                RenderCore::SetMaterialTexture(materialID, 4, texID);
              }
            }
            ImGui::EndDragDropTarget();
          }
        }
      }
    }
    break;

  case 4: // Light
    if (nodeType == "PointLightNode") {
      RenderPointLightInspector(static_cast<PointLightNode *>(s_SelectedNode));
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
                     5.0f);
    ImGui::DragFloat("Threshold##Bloom", &settings.bloomThreshold, 0.01f, 0.0f,
                     5.0f);
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

  // 使用两栏布局：左侧显示选中文件信息，右侧显示文件列表
  ImGui::Columns(2, "ContentBrowserColumns", true);
  ImGui::SetColumnWidth(0, 200.0f);

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
        std::string filename = entry.path().filename().string();
        bool isSelected = (s_SelectedFile == filename);

        if (ImGui::Selectable(filename.c_str(), isSelected,
                              ImGuiSelectableFlags_AllowDoubleClick)) {
          s_SelectedFile = filename;
        }

        // 拖拽源 (文件)
        if (ImGui::BeginDragDropSource()) {
          std::string pathStr = entry.path().string();
          ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", pathStr.c_str(),
                                    pathStr.length() + 1);
          ImGui::Text("%s", filename.c_str());
          ImGui::EndDragDropSource();
        }

        // Right-click context menu for files
        if (ImGui::BeginPopupContextItem(("文件上下文##" + filename).c_str())) {
          std::filesystem::path filePath =
              std::filesystem::path(s_CurrentPath) / filename;

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
            s_ClipboardPath = filePath.string();
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
              LOG_I("Moved file {} to {}", srcPath.string(), dstPath.string());
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
              LOG_I("Created copy: {}", dstPath.string());
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
                    AssetManager::GetInstance().GetAssetGUID(filePath.string());

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
          std::string fullPath = entry.path().string();
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
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "错误: %s", e.what());
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

  // 收集当前场景的文件名
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
