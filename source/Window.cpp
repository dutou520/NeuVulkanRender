#include "Window.h"
#include "neuLog.h"
#include <SDL3/SDL_video.h>
#include <imgui_impl_sdl3.h>

namespace neurender {

// Initialize static member variables
SDL_Window *Window::m_Window = nullptr;
bool Window::m_ShouldClose = false;
int Window::m_Width = 0;
int Window::m_Height = 0;

/**
 * @brief Initializes the SDL window and Vulkan-related settings.
 * @param width The width of the window.
 * @param height The height of the window.
 * @param title The title of the window.
 */

void Window::Init(int width, int height, const char *title) {
  // Initialize SDL video subsystem
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    LOG_E("Failed to Initialize SDL: {0}", SDL_GetError());
    return;
  }

  // Define window flags: enable Vulkan support, resizable window, and high-DPI
  // support
  SDL_WindowFlags window_flags =
      (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY);

  // Create the SDL window
  m_Window = SDL_CreateWindow(title, width, height, window_flags);
  if (!m_Window) {
    LOG_E("Failed to Create Window: {0}", SDL_GetError());
    return;
  }

  // Update window state

  m_Width = width;
  m_Height = height;
  m_ShouldClose = false;

  LOG_I("Window initialized successfully: {0}x{1}", width, height);
}

/**
 * @brief Cleans up and destroys the SDL window and shuts down SDL.
 */

void Window::Shutdown() {
  if (m_Window) {
    SDL_DestroyWindow(m_Window);
    m_Window = nullptr;
  }
  SDL_Quit();
  LOG_I("Window system shutdown");
}

/**
 * @brief Polls and processes pending SDL events.
 */

void Window::PollEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL3_ProcessEvent(&event);
    // Handle application quit event
    if (event.type == SDL_EVENT_QUIT) {
      m_ShouldClose = true;
    }
    // Handle window close request
    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
        event.window.windowID == SDL_GetWindowID(m_Window)) {
      m_ShouldClose = true;
    }
  }
}

} // namespace neurender
