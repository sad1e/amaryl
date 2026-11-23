#include "amaryl/engine/project/project.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "amaryl/core/filesystem/path_utils.h"
#include "amaryl/core/filesystem/vfs.h"
#include "boost/ut.hpp"

using namespace amaryl;
using namespace boost::ut;

namespace {

namespace fs = std::filesystem;

// "测试", the project root may contain non-ASCII characters.
constexpr const char* kNonAscii = "\xE6\xB5\x8B\xE8\xAF\x95";

// A fresh directory in the temp directory, removed on destruction.
struct TempDir {
  TempDir() {
    path = fs::temp_directory_path() /
           ToNativePath(std::string("amaryl-project-test-") + kNonAscii);
    fs::remove_all(path);
    fs::create_directories(path);
  }
  ~TempDir() {
    std::error_code error;
    fs::remove_all(path, error);
  }

  void write(std::string_view relative, std::string_view text) const {
    const fs::path file = path / ToNativePath(relative);
    fs::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary).write(text.data(), text.size());
  }

  fs::path path;
};

bool samePath(std::string_view utf8, const fs::path& expected) {
  std::error_code error;
  return fs::equivalent(ToNativePath(utf8), expected, error);
}

}  // namespace

int main() {
  "project file extension"_test = [] {
    expect(IsProjectFile("MyGame.amarylproject"));
    expect(IsProjectFile("D:\\Games\\MyGame\\MyGame.AmarylProject"));
    expect(!IsProjectFile("MyGame.amarylproject.bak"));
    expect(!IsProjectFile("amarylproject"));
    expect(!IsProjectFile("--project"));
  };

  "parse project json"_test = [] {
    const auto project = ParseProject(
      R"({ "name": "My Game", "engine_version": "0.1", "unknown": [1, 2] })",
      "D:/Games/MyGame/MyGame.amarylproject");
    expect(project.has_value());
    expect(project->name == "My Game");
    expect(project->engine_version == "0.1");
    expect(project->file_path == "D:/Games/MyGame/MyGame.amarylproject");
    expect(project->root_path == "D:/Games/MyGame");
  };

  "missing name defaults to the file stem"_test = [] {
    const auto project = ParseProject("\xEF\xBB\xBF{}", "D:/Games/MyGame/Shooter.amarylproject");
    expect(project.has_value());
    expect(project->name == "Shooter");
    expect(project->engine_version.empty());
  };

  "invalid project json is rejected"_test = [] {
    const char* file = "D:/MyGame/MyGame.amarylproject";
    expect(!ParseProject("", file).has_value());
    expect(!ParseProject("{ \"name\": ", file).has_value());
    expect(!ParseProject("[]", file).has_value());
    expect(!ParseProject(R"({ "name": 42 })", file).has_value());
    expect(!ParseProject(R"({ "name": "" })", file).has_value());
  };

  "load from the project file or its folder"_test = [] {
    const TempDir dir;
    dir.write("MyGame.amarylproject", std::string(R"({ "name": ")") + kNonAscii + "\" }");
    const fs::path file = dir.path / "MyGame.amarylproject";

    const auto from_file = LoadProject(ToUtf8(file));
    expect(from_file.has_value());
    expect(from_file->name == kNonAscii);
    expect(samePath(from_file->file_path, file));
    expect(samePath(from_file->root_path, dir.path));

    const auto from_folder = LoadProject(ToUtf8(dir.path));
    expect(from_folder.has_value());
    expect(samePath(from_folder->file_path, file));
    expect(samePath(from_folder->root_path, dir.path));
  };

  "load fails without a single project file"_test = [] {
    const TempDir dir;
    expect(!LoadProject(ToUtf8(dir.path)).has_value());
    expect(!LoadProject(ToUtf8(dir.path / "Missing.amarylproject")).has_value());

    dir.write("readme.txt", "not a project");
    expect(!LoadProject(ToUtf8(dir.path / "readme.txt")).has_value());

    dir.write("A.amarylproject", "{}");
    dir.write("B.amarylproject", "{}");
    expect(!LoadProject(ToUtf8(dir.path)).has_value());
  };

  "mount the project"_test = [] {
    const TempDir dir;
    dir.write("MyGame.amarylproject", "{}");
    dir.write("assets/models/a.gltf", "gltf");

    const auto project = LoadProject(ToUtf8(dir.path));
    expect(project.has_value());
    MountProject(*project);

    VirtualFileSystem& vfs = GetVirtualFileSystem();
    std::vector<uint8> bytes;
    expect(vfs.readFile("project://models/a.gltf", bytes));
    expect(std::string(bytes.begin(), bytes.end()) == "gltf");
    expect(!vfs.writeFile("project://models/b.gltf", bytes));

    expect(vfs.writeFile("cache://meshes/a.bin", bytes));
    expect(fs::is_regular_file(dir.path / ".amaryl" / "cache" / "meshes" / "a.bin"));

    UnmountProject();
    expect(!vfs.isMounted(kProjectScheme));
    expect(!vfs.isMounted(kCacheScheme));
  };
}
