#include "amaryl/app/editor.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>

#include "amaryl/engine/engine.h"

namespace amaryl {

bool EditorApp::initialize(const AppInitInfo& info) {
  if (!App::initialize(info)) {
    return false;
  }
  return initImGui();
}

bool EditorApp::initImGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;

  const float scale = getWindowMetrics().display_scale;
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(scale);
  style.FontScaleDpi = scale;

  if (!ImGui_ImplSDL3_InitForVulkan(getWindow())) {
    ALOGE("ImGui_ImplSDL3_InitForVulkan failed");
    ImGui::DestroyContext();
    return false;
  }
  if (!kEngine->initImGui()) {
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    return false;
  }
  imgui_initialized_ = true;
  return true;
}

void EditorApp::shutdownImGui() {
  if (!imgui_initialized_) {
    return;
  }
  // The renderer releases the ImGui textures, before the context is destroyed.
  if (kEngine) {
    kEngine->shutdownImGui();
  }
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  imgui_initialized_ = false;
}

bool EditorApp::mountFileSystem() {
  if (!App::mountFileSystem()) {
    return false;
  }
  if (project) {
    MountProject(*project);
    ALOGI("Project '%s' at '%s'", project->name.c_str(), project->root_path.c_str());
  }
  return true;
}

void EditorApp::shutdown() {
  shutdownImGui();
  UnmountProject();
  App::shutdown();
}

void EditorApp::processEvent(const SDL_Event& event) {
  if (imgui_initialized_) {
    ImGui_ImplSDL3_ProcessEvent(&event);
  }
  App::processEvent(event);
}

bool EditorApp::prepareFrame(float delta_time) {
  (void)delta_time;
  if (!imgui_initialized_) {
    return true;
  }

  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  // The central node is transparent, the scene stays visible behind the docked panels.
  ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
  drawMenubar();
  ImGui::Render();
  return true;
}

bool EditorApp::renderFrame(float delta_time) {
  (void)delta_time;
  return kEngine->render(imgui_initialized_ ? ImGui::GetDrawData() : nullptr);
}

void EditorApp::drawMenubar() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Exit")) {
        requestQuit();
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
      ImGui::MenuItem("ImGui Demo", nullptr, &show_demo_window_);
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }
  if (show_demo_window_) {
    ImGui::ShowDemoWindow(&show_demo_window_);
  }
}

}  // namespace amaryl
