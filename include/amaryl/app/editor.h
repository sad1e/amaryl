#pragma once

#include <optional>

#include "amaryl/app/app.h"
#include "amaryl/engine/project/project.h"

namespace amaryl {

struct EditorApp : App {
  // Mounted as project:// and cache:// on initialize, the editor runs without project
  // when empty.
  std::optional<ProjectInfo> project;

 protected:
  bool mountFileSystem() override;

 public:
  void shutdown() override;
};

}  // namespace amaryl
