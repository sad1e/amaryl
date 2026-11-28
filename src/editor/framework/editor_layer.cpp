#include "amaryl/editor/framework/editor_layer.h"

#include <imgui_impl_sdl3.h>

#include "amaryl/editor/framework/editor.h"
#include "amaryl/editor/level/level_context.h"
#include "amaryl/editor/scene/editor_scene.h"
#include "amaryl/engine/engine.h"

namespace amaryl::editor {

EditorLayer::EditorLayer() = default;

EditorLayer::~EditorLayer() { shutdown(); }

bool EditorLayer::init(const EditorLayerDesc& desc) {
  if (imgui_initialized_) {
    return true;
  }
  if (!kEngine || !initImGui(desc)) {
    return false;
  }
  request_quit_ = desc.request_quit;
  initEditor();
  return true;
}

void EditorLayer::initEditor() {
  scene_ = std::make_unique<EditorScene>(kEngine->getWorld(), kEngine->getModels());
  editor_ = std::make_unique<Editor>(kEngine->getWorld());
  ActionRegistry& actions = editor_->getActions();
  // vvv test import file.
  actions.add({.id = "app.import_test",
    .label = "Import Test Model",
    .menu = "File",
    .execute = [this] {
      const EntityHandle root =
        scene_->importModel("../../EngineAssets/DamagedHelmet/DamagedHelmet.gltf");
      if (!root) {
        ALOGE("Cannot import the test model");
        return;
      }
      kEngine->getWorld().sync();
      const TypeDesc* entity_type = editor_->getTypes().find(kEntityTypeName);
      if (entity_type) {
        editor_->getSelection().set(
          editor_->getObjects().getOrAdd(*entity_type, ToNative(root)));
      }
    }});
  // ^^^ test import file.
  actions.add(
    {.id = "app.exit", .label = "Exit", .menu = "File", .order = 900, .execute = [this] {
       if (request_quit_) {
         request_quit_();
       }
     }});
  editor_->addContext<LevelEditorContext>(
    [this](EntityHandle entity) { return scene_->getName(entity); });
}

bool EditorLayer::initImGui(const EditorLayerDesc& desc) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;

  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(desc.display_scale);
  style.FontScaleDpi = desc.display_scale;

  if (!ImGui_ImplSDL3_InitForVulkan(desc.window)) {
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

void EditorLayer::shutdown() {
  editor_.reset();
  // The destructor collects the garbage of the world, before the engine shuts down.
  scene_.reset();
  shutdownImGui();
  request_quit_ = {};
}

void EditorLayer::shutdownImGui() {
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

void EditorLayer::processEvent(const SDL_Event& event) {
  if (imgui_initialized_) {
    ImGui_ImplSDL3_ProcessEvent(&event);
  }
}

void EditorLayer::buildFrame(float delta_time) {
  if (!imgui_initialized_) {
    return;
  }
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  // note: disable editor for now
  editor_->drawFrame(delta_time);
  (void)delta_time;
  ImGui::Render();
}

ImDrawData* EditorLayer::getDrawData() const {
  return imgui_initialized_ ? ImGui::GetDrawData() : nullptr;
}

}  // namespace amaryl::editor
