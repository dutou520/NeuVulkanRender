#pragma once
#include "neuwindow_export.h"
#include <SDL3/SDL.h>

namespace neurender {
class NEUWINDOW_API Window {
public:
  Window() = delete;
  Window(const Window &) = delete;
  Window &operator=(const Window &) = delete;
  static void Init(int width, int height, const char *title);
  static void Shutdown();
  static bool ShouldClose() { return m_ShouldClose; }
  static void PollEvents();
  static void BeginFrame() {
  } // SDL doesn't really have a begin frame for window itself unless we do
    // something specific, but RenderCore will handle the big picture. Keeping
    // empty or removing if not needed. Actually, let's keep it simple.
  static void EndFrame() {}

  static SDL_Window *GetNativeWindow() { return m_Window; }
  static int GetWidth() { return m_Width; }
  static int GetHeight() { return m_Height; }
  static void Close() { m_ShouldClose = true; }
  static bool IsCloseRequested() { return m_CloseRequested; }
  static void RequestClose() { m_CloseRequested = true; }
  static void ResetCloseRequest() { m_CloseRequested = false; }
  static bool IsMinimized();
  static bool IsGUIVisible() { return m_ShowGUI; }
  static void SetGUIVisible(bool visible) { m_ShowGUI = visible; }
  static void Restart(const char *args = nullptr);

private:
  static SDL_Window *m_Window;
  static bool m_ShouldClose;
  static bool m_CloseRequested;
  static int m_Width;
  static int m_Height;
  static bool m_ShowGUI;
};
} // namespace neurender
