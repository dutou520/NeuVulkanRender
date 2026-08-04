#include "Console.h"
#include "Nodes/MeshNode.h"
#include "Nodes/Node.h"
#include "Project/Project.h"
#include "RenderCore.h"
#include "Scene/Scene.h"
#include "Window.h"
#include "neuGUI.h"
#include "neuLog.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <glm/glm.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace neurender {

// ========== 静态成员 ==========
std::vector<Console::CommandEntry> Console::s_Commands;
std::vector<std::string> Console::s_PendingCommands;
std::mutex Console::s_QueueMutex;
std::thread Console::s_InputThread;
bool Console::s_StopRequested = false;
bool Console::s_Initialized = false;

// ========== 工具函数 ==========
namespace {

// 将一行命令拆分为 token，支持双引号包裹的含空格路径
std::vector<std::string> Tokenize(const std::string &line) {
  std::vector<std::string> tokens;
  std::string token;
  bool inQuotes = false;
  for (size_t i = 0; i < line.size(); i++) {
    char c = line[i];
    if (c == '"') {
      inQuotes = !inQuotes;
    } else if (c == ' ' || c == '\t') {
      if (!inQuotes && !token.empty()) {
        tokens.push_back(token);
        token.clear();
      } else if (inQuotes) {
        token += c;
      }
    } else {
      token += c;
    }
  }
  if (!token.empty()) {
    tokens.push_back(token);
  }
  return tokens;
}

// 解析 on/off/0/1 布尔值，成功返回 true
bool ParseBool(const std::string &s, bool &outValue) {
  std::string lower = s;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (lower == "on" || lower == "1" || lower == "true" || lower == "yes") {
    outValue = true;
    return true;
  }
  if (lower == "off" || lower == "0" || lower == "false" || lower == "no") {
    outValue = false;
    return true;
  }
  return false;
}

bool ParseFloat(const std::string &s, float &outValue) {
  try {
    size_t pos = 0;
    outValue = std::stof(s, &pos);
    return pos == s.size();
  } catch (...) {
    return false;
  }
}

bool ParseInt(const std::string &s, int &outValue) {
  try {
    size_t pos = 0;
    outValue = std::stoi(s, &pos);
    return pos == s.size();
  } catch (...) {
    return false;
  }
}

// 解析 vec3（三个 float），成功返回 true
bool ParseVec3(const std::vector<std::string> &args, size_t start, glm::vec3 &out) {
  if (args.size() < start + 3) {
    return false;
  }
  float x, y, z;
  if (!ParseFloat(args[start], x) || !ParseFloat(args[start + 1], y) ||
      !ParseFloat(args[start + 2], z)) {
    return false;
  }
  out = glm::vec3(x, y, z);
  return true;
}

std::string Vec3ToString(const glm::vec3 &v) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(3) << "(" << v.x << ", " << v.y << ", "
      << v.z << ")";
  return oss.str();
}

// 归一化路径：去掉结尾的引号/反斜杠，Windows 下反斜杠转正斜杠
std::string NormalizePath(std::string path) {
  if (!path.empty() && path.back() == '"') {
    path.pop_back();
  }
  std::replace(path.begin(), path.end(), '\\', '/');
  while (path.size() > 1 && path.back() == '/') {
    path.pop_back();
  }
  return path;
}

// 常用错误输出
void LogError(const std::string &usage, const std::string &detail) {
  LOG_E("Console: {} — usage: {}", detail, usage);
}

// ========== 命令实现 ==========

bool CmdHelp(const std::vector<std::string> &args) {
  Console::Print("=== NeuVulkanRender Console Commands ===");
  for (const auto &cmd : Console::s_Commands) {
    std::ostringstream oss;
    oss << "  " << cmd.name;
    oss << std::setw(static_cast<int>(28 - cmd.name.size())) << "";
    oss << cmd.usage << " — " << cmd.description;
    Console::Print(oss.str());
  }
  Console::Print("提示: 路径含空格请用双引号包裹，例如 load project \"D:/My Projects/A\"");
  return true;
}

bool CmdQuit(const std::vector<std::string> &args) {
  Console::Print("Quit requested.");
  Window::Close();
  return true;
}

bool CmdEcho(const std::vector<std::string> &args) {
  std::ostringstream oss;
  for (size_t i = 0; i < args.size(); i++) {
    if (i > 0) oss << " ";
    oss << args[i];
  }
  Console::Print(oss.str());
  return true;
}

bool CmdSleep(const std::vector<std::string> &args) {
  if (args.empty()) {
    LogError("sleep <ms>", "缺少毫秒数");
    return false;
  }
  int ms = 0;
  if (!ParseInt(args[0], ms) || ms < 0) {
    LogError("sleep <ms>", "毫秒数格式错误");
    return false;
  }
  LOG_I("Console: sleeping {} ms ...", ms);
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
  return true;
}

bool CmdLoadProject(const std::vector<std::string> &args) {
  if (args.empty()) {
    LogError("load project <path>", "缺少工程路径");
    return false;
  }
  std::string path = NormalizePath(args[0]);
  if (!std::filesystem::exists(std::filesystem::u8path(path))) {
    LOG_E("Console: 工程路径不存在: {}", path);
    return false;
  }
  // 允许直接传 project.json 文件路径
  if (std::filesystem::path(path).filename() != "project.json" &&
      std::filesystem::exists(std::filesystem::u8path(path) / "project.json")) {
    path = (std::filesystem::u8path(path) / "project.json").u8string();
  }
  return EditorGUI::LoadProjectFromPath(path);
}

bool CmdLoadScene(const std::vector<std::string> &args) {
  if (args.empty()) {
    LogError("load scene <path>", "缺少场景路径");
    return false;
  }
  std::string path = NormalizePath(args[0]);
  return EditorGUI::LoadSceneFromPath(path);
}

bool CmdSaveProject(const std::vector<std::string> &args) {
  auto project = RenderCore::GetCurrentProject();
  if (!project) {
    LOG_E("Console: 当前没有加载工程");
    return false;
  }
  return project->Save();
}

bool CmdScreenshot(const std::vector<std::string> &args) {
  std::string path = args.empty() ? "console_screenshot.png" : NormalizePath(args[0]);
  RenderCore::RequestScreenshot(path);
  LOG_I("Console: 截图请求已排队: {}", path);
  return true;
}

bool CmdInfo(const std::vector<std::string> &args) {
  auto project = RenderCore::GetCurrentProject();
  auto scene = EditorGUI::GetCurrentScene();

  std::ostringstream oss;
  oss << "=== Renderer Info ===";
  Console::Print(oss.str());

  oss.str("");
  oss << "Window: " << Window::GetWidth() << "x" << Window::GetHeight()
      << ", VSync: " << (RenderCore::IsVSyncEnabled() ? "ON" : "OFF")
      << ", TargetFPS: " << RenderCore::GetTargetFPS();
  Console::Print(oss.str());

  const auto &pp = RenderCore::GetPostProcessSettings();
  oss.str("");
  oss << "PostProcess: SSAO=" << (pp.enableSSAO ? "ON" : "OFF")
      << " Bloom=" << (pp.enableBloom ? "ON" : "OFF")
      << " ToneMap=" << (pp.enableToneMapping ? "ON" : "OFF")
      << " Gamma=" << (pp.enableGamma ? "ON" : "OFF")
      << " SSR=" << (pp.enableSSR ? "ON" : "OFF")
      << " DebugMode=" << pp.debugMode;
  Console::Print(oss.str());

  oss.str("");
  oss << "TAA: " << (RenderCore::IsTAAEnabled() ? "ON" : "OFF")
      << ", SuperResScale: " << RenderCore::GetSuperResolutionScale()
      << ", ShadowRes: " << RenderCore::GetPCSSSettings().shadowMapRes;
  Console::Print(oss.str());

  auto &camera = RenderCore::GetCamera();
  oss.str("");
  oss << "Camera: pos=" << Vec3ToString(camera.GetPosition())
      << " yaw=" << std::fixed << std::setprecision(1) << camera.GetYaw()
      << " pitch=" << camera.GetPitch() << " fov=" << camera.GetFov()
      << " near=" << camera.GetNearPlane() << " far=" << camera.GetFarPlane();
  Console::Print(oss.str());

  oss.str("");
  oss << "Project: " << (project ? project->GetName() : "(none)")
      << ", Scene: " << (scene ? scene->GetName() : "(none)");
  Console::Print(oss.str());

  const auto &sky = RenderCore::GetSkyboxSettings();
  oss.str("");
  oss << "Skybox: " << (sky.name.empty() ? "(default)" : sky.name)
      << ", Brightness: " << std::fixed << std::setprecision(2) << sky.brightness;
  Console::Print(oss.str());
  return true;
}

bool CmdSet(const std::vector<std::string> &args) {
  if (args.size() < 2) {
    LogError("set <key> <value>", "参数不足");
    return false;
  }
  const std::string &key = args[0];
  bool bval = false;
  float fval = 0.0f;
  int ival = 0;

  if (key == "vsync") {
    if (ParseBool(args[1], bval)) {
      RenderCore::SetVSync(bval);
      LOG_I("Console: VSync -> {}", bval ? "ON" : "OFF");
      return true;
    }
  } else if (key == "fps" || key == "target-fps") {
    if (ParseInt(args[1], ival)) {
      RenderCore::SetTargetFPS(ival);
      LOG_I("Console: TargetFPS -> {}", ival);
      return true;
    }
  } else if (key == "taa") {
    if (ParseBool(args[1], bval)) {
      RenderCore::SetTAAEnabled(bval);
      LOG_I("Console: TAA -> {}", bval ? "ON" : "OFF");
      return true;
    }
  } else if (key == "sr" || key == "super-resolution") {
    if (ParseFloat(args[1], fval)) {
      RenderCore::SetSuperResolutionScale(fval);
      LOG_I("Console: SuperResolutionScale -> {}", fval);
      return true;
    }
  } else if (key == "debug-mode") {
    if (ParseInt(args[1], ival)) {
      RenderCore::GetPostProcessSettings().debugMode =
          static_cast<uint32_t>(ival);
      LOG_I("Console: DebugMode -> {} (0=Shaded 1=Wireframe 2=Albedo 3=Normal "
            "4=Depth 5=Smoothness 6=Specular 7=Occlusion 8=MaterialFlags "
            "9=ShadingID 10=Emission)",
            ival);
      return true;
    }
  } else if (key == "shadow-res") {
    if (ParseInt(args[1], ival)) {
      RenderCore::SetShadowMapResolution(static_cast<uint32_t>(ival));
      LOG_I("Console: ShadowMapResolution -> {}", ival);
      return true;
    }
  } else if (key == "bloom") {
    if (ParseBool(args[1], bval)) {
      RenderCore::GetPostProcessSettings().enableBloom = bval ? 1 : 0;
      LOG_I("Console: Bloom -> {}", bval ? "ON" : "OFF");
      return true;
    }
  } else if (key == "ssao") {
    if (ParseBool(args[1], bval)) {
      RenderCore::GetPostProcessSettings().enableSSAO = bval ? 1 : 0;
      LOG_I("Console: SSAO -> {}", bval ? "ON" : "OFF");
      return true;
    }
  } else if (key == "ssr") {
    if (ParseBool(args[1], bval)) {
      RenderCore::GetPostProcessSettings().enableSSR = bval ? 1 : 0;
      LOG_I("Console: SSR -> {}", bval ? "ON" : "OFF");
      return true;
    }
  } else if (key == "tonemap" || key == "tone-mapping") {
    if (ParseBool(args[1], bval)) {
      RenderCore::GetPostProcessSettings().enableToneMapping = bval ? 1 : 0;
      LOG_I("Console: ToneMapping -> {}", bval ? "ON" : "OFF");
      return true;
    }
  } else if (key == "gamma") {
    if (ParseBool(args[1], bval)) {
      RenderCore::GetPostProcessSettings().enableGamma = bval ? 1 : 0;
      LOG_I("Console: Gamma -> {}", bval ? "ON" : "OFF");
      return true;
    }
  } else if (key == "fov") {
    if (ParseFloat(args[1], fval)) {
      RenderCore::GetCamera().SetFov(fval);
      LOG_I("Console: FOV -> {}", fval);
      return true;
    }
  } else if (key == "speed") {
    if (ParseFloat(args[1], fval)) {
      RenderCore::GetCamera().SetMovementSpeed(fval);
      LOG_I("Console: CameraSpeed -> {}", fval);
      return true;
    }
  } else if (key == "sens" || key == "sensitivity") {
    if (ParseFloat(args[1], fval)) {
      RenderCore::GetCamera().SetMouseSensitivity(fval);
      LOG_I("Console: CameraSensitivity -> {}", fval);
      return true;
    }
  } else if (key == "near") {
    if (ParseFloat(args[1], fval)) {
      RenderCore::GetCamera().SetNearPlane(fval);
      LOG_I("Console: NearPlane -> {}", fval);
      return true;
    }
  } else if (key == "far") {
    if (ParseFloat(args[1], fval)) {
      RenderCore::GetCamera().SetFarPlane(fval);
      LOG_I("Console: FarPlane -> {}", fval);
      return true;
    }
  } else if (key == "light-dir") {
    glm::vec3 dir;
    if (ParseVec3(args, 1, dir)) {
      auto &settings = RenderCore::GetPCSSSettings();
      if (glm::length(dir) > 1e-6f) {
        settings.lightDirection = glm::normalize(dir);
      }
      LOG_I("Console: LightDirection -> {}", Vec3ToString(settings.lightDirection));
      return true;
    }
  } else if (key == "skybox-brightness") {
    if (ParseFloat(args[1], fval)) {
      RenderCore::GetSkyboxSettings().brightness = fval;
      RenderCore::ReloadSkybox();
      LOG_I("Console: SkyboxBrightness -> {}", fval);
      return true;
    }
  } else if (key == "camera-control") {
    if (ParseBool(args[1], bval)) {
      RenderCore::SetCameraControlEnabled(bval);
      LOG_I("Console: CameraControl -> {}", bval ? "ON" : "OFF");
      return true;
    }
  } else {
    LOG_E("Console: 未知设置项: {}（输入 help 查看所有命令）", key);
    return false;
  }

  LogError("set <key> <value>", "值格式错误: " + args[1]);
  return false;
}

bool CmdCamera(const std::vector<std::string> &args) {
  if (args.empty()) {
    LogError("camera pos <x> <y> <z> | camera yawpitch <yaw> <pitch> | camera reset",
             "缺少子命令");
    return false;
  }
  auto &camera = RenderCore::GetCamera();
  if (args[0] == "pos") {
    glm::vec3 pos;
    if (!ParseVec3(args, 1, pos)) {
      LogError("camera pos <x> <y> <z>", "坐标格式错误");
      return false;
    }
    camera.SetPosition(pos);
    LOG_I("Console: Camera position -> {}", Vec3ToString(pos));
    return true;
  }
  if (args[0] == "yawpitch") {
    if (args.size() < 3) {
      LogError("camera yawpitch <yaw> <pitch>", "参数不足");
      return false;
    }
    float yaw, pitch;
    if (!ParseFloat(args[1], yaw) || !ParseFloat(args[2], pitch)) {
      LogError("camera yawpitch <yaw> <pitch>", "角度格式错误");
      return false;
    }
    camera.SetYaw(yaw);
    camera.SetPitch(pitch);
    LOG_I("Console: Camera yaw/pitch -> {:.1f} / {:.1f}", yaw, pitch);
    return true;
  }
  if (args[0] == "reset") {
    camera.Reset();
    LOG_I("Console: Camera reset");
    return true;
  }
  LogError("camera pos <x> <y> <z> | camera yawpitch <yaw> <pitch> | camera reset",
           "未知子命令: " + args[0]);
  return false;
}

bool CmdNodes(const std::vector<std::string> &args) {
  auto scene = EditorGUI::GetCurrentScene();
  if (!scene) {
    LOG_E("Console: 当前没有加载场景");
    return false;
  }
  Console::Print("=== Nodes in scene: " + scene->GetName() + " ===");
  scene->TraverseNodes([](Node *node) {
    std::string indent = "  ";
    Node *parent = node->GetParent();
    while (parent) {
      indent += "  ";
      parent = parent->GetParent();
    }
    LOG_I("Console: {}{} [{}] pos={} active={}", indent, node->GetName(),
          node->GetNodeType(), Vec3ToString(node->GetPosition()),
          node->IsActive() ? "true" : "false");
  });
  return true;
}

bool CmdNode(const std::vector<std::string> &args) {
  if (args.size() < 2) {
    LogError("node info|move|rotate|scale <name> [...]", "参数不足");
    return false;
  }
  auto scene = EditorGUI::GetCurrentScene();
  if (!scene) {
    LOG_E("Console: 当前没有加载场景");
    return false;
  }
  Node *node = scene->FindNode(args[1]);
  if (!node) {
    LOG_E("Console: 未找到节点: {}", args[1]);
    return false;
  }

  if (args[0] == "info") {
    LOG_I("Console: Node '{}': type={} pos={} rot={} scale={} children={} "
          "active={}",
          node->GetName(), node->GetNodeType(), Vec3ToString(node->GetPosition()),
          Vec3ToString(node->GetRotation()), Vec3ToString(node->GetScale()),
          node->GetChildren().size(), node->IsActive() ? "true" : "false");
    return true;
  }
  if (args[0] == "move") {
    glm::vec3 v;
    if (!ParseVec3(args, 2, v)) {
      LogError("node move <name> <x> <y> <z>", "坐标格式错误");
      return false;
    }
    node->SetPosition(v);
    LOG_I("Console: Node '{}' position -> {}", node->GetName(), Vec3ToString(v));
    return true;
  }
  if (args[0] == "rotate") {
    glm::vec3 v;
    if (!ParseVec3(args, 2, v)) {
      LogError("node rotate <name> <x> <y> <z>", "角度格式错误");
      return false;
    }
    node->SetRotation(v);
    LOG_I("Console: Node '{}' rotation -> {}", node->GetName(), Vec3ToString(v));
    return true;
  }
  if (args[0] == "scale") {
    glm::vec3 v;
    if (!ParseVec3(args, 2, v)) {
      LogError("node scale <name> <x> <y> <z>", "缩放格式错误");
      return false;
    }
    node->SetScale(v);
    LOG_I("Console: Node '{}' scale -> {}", node->GetName(), Vec3ToString(v));
    return true;
  }

  LogError("node info|move|rotate|scale <name> [...]", "未知子命令: " + args[0]);
  return false;
}

} // namespace

// ========== Console 实现 ==========

void Console::Init() {
  if (s_Initialized) {
    return;
  }
  s_Initialized = true;
  s_StopRequested = false;

  RegisterCommand("help", "help", "显示所有命令及用法", CmdHelp);
  RegisterCommand("quit", "quit|exit", "退出程序", CmdQuit);
  RegisterCommand("exit", "quit|exit", "退出程序", CmdQuit);
  RegisterCommand("echo", "echo <text>", "输出文本", CmdEcho);
  RegisterCommand("sleep", "sleep <ms>", "暂停指定毫秒数（用于脚本时序控制）",
                  CmdSleep);
  RegisterCommand("load", "load project <path> | load scene <path>", "加载工程或场景",
                  [](const std::vector<std::string> &args) {
                    if (args.empty()) {
                      LogError("load project <path> | load scene <path>",
                               "缺少参数");
                      return false;
                    }
                    if (args[0] == "project") {
                      return CmdLoadProject(
                          std::vector<std::string>(args.begin() + 1, args.end()));
                    }
                    if (args[0] == "scene") {
                      return CmdLoadScene(
                          std::vector<std::string>(args.begin() + 1, args.end()));
                    }
                    LogError("load project <path> | load scene <path>",
                             "未知子命令: " + args[0]);
                    return false;
                  });
  RegisterCommand("load-project", "load-project <path>", "加载工程 (快捷方式)",
                  CmdLoadProject);
  RegisterCommand("load-scene", "load-scene <path>", "加载场景 (快捷方式)",
                  CmdLoadScene);
  RegisterCommand("save", "save project", "保存当前工程", CmdSaveProject);
  RegisterCommand("screenshot", "screenshot [path]", "保存截图 (默认 console_screenshot.png)",
                  CmdScreenshot);
  RegisterCommand("info", "info", "显示渲染器/相机/场景状态", CmdInfo);
  RegisterCommand("set", "set <key> <value>", "修改渲染参数 (详见 help)", CmdSet);
  RegisterCommand("camera", "camera pos <x y z> | yawpitch <yaw pitch> | reset",
                  "控制编辑器相机", CmdCamera);
  RegisterCommand("nodes", "nodes", "列出当前场景所有节点", CmdNodes);
  RegisterCommand("node", "node info|move|rotate|scale <name> [...]",
                  "查询/修改节点", CmdNode);

  s_InputThread = std::thread(InputThreadFunc);
  LOG_I("Console: 命令行调试系统已启动（在终端输入 help 查看命令）");
}

void Console::Shutdown() {
  s_StopRequested = true;
  if (s_InputThread.joinable()) {
    // getline 阻塞时无法安全 join，进程即将退出，直接分离
    s_InputThread.detach();
  }
}

void Console::Update() {
  // 每帧只执行一条命令，保证 piped 脚本中命令间的时序（截图等按帧执行）
  std::string line;
  {
    std::lock_guard<std::mutex> lock(s_QueueMutex);
    if (s_PendingCommands.empty()) {
      return;
    }
    line = s_PendingCommands.front();
    s_PendingCommands.erase(s_PendingCommands.begin());
  }

  auto tokens = Tokenize(line);
  if (tokens.empty()) {
    return;
  }
  std::string cmdName = tokens[0];
  std::transform(cmdName.begin(), cmdName.end(), cmdName.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  auto it = std::find_if(s_Commands.begin(), s_Commands.end(),
                         [&](const CommandEntry &entry) {
                           return entry.name == cmdName;
                         });
  if (it == s_Commands.end()) {
    LOG_E("Console: 未知命令: {}（输入 help 查看所有命令）", tokens[0]);
    return;
  }
  std::vector<std::string> cmdArgs(tokens.begin() + 1, tokens.end());
  LOG_I("Console: >>> {}", line);
  try {
    it->handler(cmdArgs);
  } catch (const std::exception &e) {
    LOG_E("Console: 命令执行异常: {}", e.what());
  }
}

void Console::RegisterCommand(const std::string &name, const std::string &usage,
                              const std::string &description,
                              CommandHandler handler) {
  s_Commands.push_back({name, usage, description, std::move(handler)});
}

void Console::Execute(const std::string &commandLine) {
  std::lock_guard<std::mutex> lock(s_QueueMutex);
  s_PendingCommands.push_back(commandLine);
}

void Console::Print(const std::string &message) {
  // 与 spdlog 控制台输出保持一致，直接写 stdout
  std::lock_guard<std::mutex> lock(s_QueueMutex);
  printf("%s\n", message.c_str());
  fflush(stdout);
}

void Console::InputThreadFunc() {
  std::string line;
  while (!s_StopRequested && std::getline(std::cin, line)) {
    if (!line.empty()) {
      std::lock_guard<std::mutex> lock(s_QueueMutex);
      s_PendingCommands.push_back(line);
    }
  }
}

} // namespace neurender
