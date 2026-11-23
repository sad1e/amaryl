#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "amaryl/engine/common.h"

namespace amaryl {

// A project is a folder with a single `<Name>.amarylproject` file, the folder is the
// project root:
//
//   MyGame/                    project root
//   ├── MyGame.amarylproject   JSON: { "name": "My Game", "engine_version": "0.1" }
//   ├── assets/                project://
//   └── .amaryl/cache/         cache://
inline constexpr std::string_view kProjectFileExtension = ".amarylproject";

struct ProjectInfo {
  // "name" in the project file, the file stem when it is missing.
  std::string name;
  std::string engine_version;
  // Absolute native UTF-8 paths: ".../MyGame/MyGame.amarylproject" and ".../MyGame".
  std::string file_path;
  std::string root_path;
};

// True when the extension of `path` is ".amarylproject", case insensitive.
ENGINE_API bool IsProjectFile(std::string_view path);

// `path` is a native UTF-8 path to a project file or to a folder containing exactly one.
// A relative path is resolved against the working directory, only pass user input (e.g.
// a command line argument). Returns nullopt and logs when the project cannot be loaded.
ENGINE_API std::optional<ProjectInfo> LoadProject(std::string_view path);
// Parses the content of a project file, `file_path` is the native UTF-8 path it was read
// from and gives the project root.
ENGINE_API std::optional<ProjectInfo> ParseProject(
  std::string_view json, std::string_view file_path);

// Mounts project:// on `<root>/assets` and the writable cache:// on `<root>/.amaryl/cache`
// in the global virtual file system, replacing the previous project.
ENGINE_API void MountProject(const ProjectInfo& project);
ENGINE_API void UnmountProject();

}  // namespace amaryl
