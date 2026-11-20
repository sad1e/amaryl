#include "amaryl/app/editor.h"

#include <SDL3/SDL_main.h>

#include "amaryl/app/app.h"

namespace {

std::unique_ptr<amaryl::App> app;
uint32 frame_limit = 0;
uint32 frame_count = 0;
std::string screenshot_path;

}  // namespace

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
  // InitializeDefaultAllocator();
  amaryl::InitializeLogger();

  bool hidden = false;
  for (int index = 1; index < argc; ++index) {
    if (std::string_view(argv[index]) == "--frames" && index + 1 < argc) {
      frame_limit = static_cast<uint32>(std::strtoul(argv[++index], nullptr, 10));
    } else if (std::string_view(argv[index]) == "--screenshot" && index + 1 < argc) {
      screenshot_path = argv[++index];
    } else if (std::string_view(argv[index]) == "--hidden") {
      hidden = true;
    }
  }
  app = std::make_unique<amaryl::App>();
  if (!app->initialize({.name = "App",
                        .width = amaryl::kDefaultWidth,
                        .height = amaryl::kDefaultHeight,
                        .hidden = hidden})) {
    return SDL_APP_FAILURE;
  }
  *appstate = app.get();
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
