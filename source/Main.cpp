#include "RenderCore.h"
#include "Window.h"
#include "neuLog.h"
#include <spdlog/spdlog.h>

int main(int argc, char *argv[]) {
  // Set global locale to UTF-8 for robust filesystem handling on Windows
  try {
    std::locale::global(std::locale(".UTF8"));
  } catch (...) {
    // Fallback if .UTF8 is not supported
  }

  neurender::NeuLog::Init();
  LOG_I("Starting NeuVulkanRender...");

  neurender::Window::Init(1920, 1080, "NeuVulkanRender Engine");

  try {
    neurender::RenderCore::Init();
  } catch (const std::exception &e) {
    LOG_E("RenderCore Init Failed: {0}", e.what());
    return -1;
  }

  while (!neurender::Window::ShouldClose()) {
    neurender::Window::PollEvents();
    try {
      neurender::RenderCore::DrawFrame();
    } catch (const std::exception &e) {
      LOG_E("Render Loop Error: {0}", e.what());
      break;
    }
  }

  neurender::RenderCore::Shutdown();
  neurender::Window::Shutdown();

  LOG_I("Application exited cleanly.");
  spdlog::shutdown();

  return 0;
}
