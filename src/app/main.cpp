#include <SDL3/SDL_main.h>

#include <cstdlib>
#include <memory>

#include "amaryl/app/app.h"

namespace {

#if AMARYL_WITH_EDITOR
constexpr const char* kAppName = "Amaryl Editor";
#else
constexpr const char* kAppName = "Amaryl";
#endif

std::unique_ptr<amaryl::App> app;
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

  app = std::make_unique<amaryl::App>();
  if (!project_path.empty()) {
    app->project = amaryl::LoadProject(project_path);
    if (!app->project) {
      return SDL_APP_FAILURE;
    }
  }
  if (!app->initialize({.name = kAppName,
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
