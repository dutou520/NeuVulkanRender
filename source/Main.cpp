#include "RenderCore.h"
#include "Window.h"
#include "neuLog.h"
#include <iostream>
#include <spdlog/spdlog.h>

int main(int argc, char *argv[]) {
  neurender::NeuLog::Init();
  LOG_I("Starting NeuVulkanRender...");

  neurender::Window::Init(1280, 720, "NeuVulkanRender Engine");

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
