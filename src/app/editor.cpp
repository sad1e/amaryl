#include "amaryl/app/editor.h"

#include <SDL3/SDL_main.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "amaryl/app/app.h"
#include "amaryl/core/filesystem/vfs.h"

namespace amaryl {

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
  UnmountProject();
  App::shutdown();
}

}  // namespace amaryl

namespace {

std::unique_ptr<amaryl::EditorApp> app;
uint32 frame_limit = 0;
uint32 frame_count = 0;
std::string screenshot_path;
}  // namespace

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
  // InitializeDefaultAllocator();
  amaryl::InitializeLogger();

  bool hidden = false;
  std::string project_path;
  for (int index = 1; index < argc; ++index) {
    const std::string_view arg(argv[index]);
    if (arg == "--frames" && index + 1 < argc) {
      frame_limit = static_cast<uint32>(std::strtoul(argv[++index], nullptr, 10));
    } else if (arg == "--screenshot" && index + 1 < argc) {
      screenshot_path = argv[++index];
    } else if (arg == "--hidden") {
      hidden = true;
    } else if (arg == "--project" && index + 1 < argc) {
      project_path = argv[++index];
    } else if (amaryl::IsProjectFile(arg)) {
      // Opening a .amarylproject from the explorer passes its path alone.
      project_path = arg;
    }
  }

  app = std::make_unique<amaryl::EditorApp>();
  if (!project_path.empty()) {
    app->project = amaryl::LoadProject(project_path);
    if (!app->project) {
      return SDL_APP_FAILURE;
    }
  }
  if (!app->initialize({.name = "Amaryl Editor",
        .width = amaryl::kDefaultWidth,
        .height = amaryl::kDefaultHeight,
        .hidden = hidden})) {
    return SDL_APP_FAILURE;
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  if (app && app->isInitialized()) {
    app->processEvent(*event);
    return app->isQuitRequested() ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  if (!app || !app->tick()) {
    return SDL_APP_FAILURE;
  }

  if (frame_limit && ++frame_count >= frame_limit) {
    // if (!screenshot_path.empty() && !app->captureScreenshot(screenshot_path)) {
    //   return SDL_APP_FAILURE;
    // }
    return SDL_APP_SUCCESS;
  }
  return app->isQuitRequested() ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
  if (app) {
    app->shutdown();
    app.reset();
  }
  // ReleaseDefaultAllocator();
  SDL_Quit();
}
