#include "Project/Project.h"
#include "RenderCore.h"
#include "Window.h"
#include "Console.h"
#include "neuGUI.h"
#include "neuLog.h"
#include <spdlog/spdlog.h>
#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif

void setConsoleUtf8Encoding() {
#if defined(_WIN32) || defined(_WIN64)
  SetConsoleOutputCP(CP_UTF8);
#else
  // Linux/macOS：终端默认UTF-8，无需额外设置
#endif
}

int main(int argc, char *argv[]) {
  setConsoleUtf8Encoding();
  // Set global locale to UTF-8 for robust filesystem handling on Windows
  try {
    std::locale::global(std::locale(".UTF8"));
  } catch (...) {
    // Fallback if .UTF8 is not supported
  }

  neurender::NeuLog::Init();
  LOG_I("Starting NeuVulkanRender...");

  neurender::Window::Init(1600, 900, "NeuVulkanRender");

  neurender::RenderCore::LoadGlobalSettings();

  // 命令行参数可传工程路径（目录或 project.json），
  // 在 RenderCore::Init() 之后执行，确保 EditorGUI 就绪并可加载场景
  std::string startupProjectPath;
  if (argc > 1) {
    startupProjectPath = argv[1];
    LOG_I("Loading project from command line: {}", startupProjectPath);
  }

  try {
    neurender::RenderCore::Init();

    // Set project after Init
    if (!startupProjectPath.empty()) {
      neurender::EditorGUI::LoadProjectFromPath(startupProjectPath);
    }
  } catch (const std::exception &e) {
    LOG_E("RenderCore Init Failed: {0}", e.what());
    return -1;
  }

  // 启动命令行调试系统（stdin 监听线程 + 命令队列）
  neurender::Console::Init();

  while (!neurender::Window::ShouldClose()) {
    neurender::Window::PollEvents();
    try {
      neurender::Console::Update(); // 在渲染线程上执行控制台命令
      neurender::RenderCore::DrawFrame();
    } catch (const std::exception &e) {
      LOG_E("Render Loop Error: {0}", e.what());
      break;
    }
  }

  neurender::Console::Shutdown();
  neurender::RenderCore::Shutdown();
  neurender::Window::Shutdown();

  LOG_I("Application exited cleanly.");
  spdlog::shutdown();

  return 0;
}
