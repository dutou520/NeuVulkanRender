#include "neuGUI.h"
#include "Window.h"
#include "neuLog.h"
#include <imgui.h>

namespace neurender {

void neuGUI::Render() {
  // Setup a specific window on the right side
  ImGuiIO &io = ImGui::GetIO();
  float width = 300.0f;
  float height = (float)Window::GetHeight();

  ImGui::SetNextWindowPos(ImVec2((float)Window::GetWidth() - width, 0.0f));
  ImGui::SetNextWindowSize(ImVec2(width, height));
  // 名称
  ImGui::Begin("Debug Panel", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

  ImGui::Text("Application Stats");
  ImGui::Text("Framerate: %.1f FPS", io.Framerate); // 占位符与c语言一致
  ImGui::Separator();

  static float bg_color[3] = {
      0.0f, 0.0f,
      0.0f}; // Not really controlling anything yet but good for show
  ImGui::ColorEdit3("Background Color", bg_color);

  ImGui::Separator();

  if (ImGui::Button("Log Info")) {
    LOG_I("Info Button Clicked from ImGui!");
  }

  if (ImGui::Button("Log Warning")) {
    LOG_W("Warning Button Clicked from ImGui!");
  }

  if (ImGui::Button("Log Error")) {
    LOG_E("Error Button Clicked from ImGui!");
  }

  ImGui::End();
}

} // namespace neurender
