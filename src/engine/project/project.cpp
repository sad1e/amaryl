#include "amaryl/engine/project/project.h"

#include <filesystem>
#include <memory>
#include <vector>

#include "amaryl/core/filesystem/path_utils.h"
#include "amaryl/core/filesystem/vfs.h"
#include "simdjson/simdjson.h"

namespace amaryl {

namespace fs = std::filesystem;

namespace {

constexpr std::string_view kUtf8Bom = "\xEF\xBB\xBF";

// The file itself, or the single project file of a folder.
std::optional<fs::path> findProjectFile(const fs::path& path) {
  std::error_code error;
  if (fs::is_regular_file(path, error)) {
    if (IsProjectFile(ToUtf8(path))) {
      return path;
    }
    ALOGE("'%s' is not a %s file", ToUtf8(path).c_str(), kProjectFileExtension.data());
    return std::nullopt;
  }
  if (!fs::is_directory(path, error)) {
    ALOGE("'%s' does not exist", ToUtf8(path).c_str());
    return std::nullopt;
  }

  std::optional<fs::path> found;
  for (fs::directory_iterator it(path, error), end; !error && it != end;
       it.increment(error)) {
    if (!it->is_regular_file(error) || !IsProjectFile(ToUtf8(it->path()))) {
      continue;
    }
    if (found) {
      ALOGE("Several project files in '%s'", ToUtf8(path).c_str());
      return std::nullopt;
    }
    found = it->path();
  }
  if (!found) {
    ALOGE("No %s file in '%s'", kProjectFileExtension.data(), ToUtf8(path).c_str());
  }
  return found;
}

// A missing key keeps `out` unchanged, any other type than a string is an error.
bool readString(const simdjson::dom::object& root, std::string_view key,
  std::string_view file_path, std::string& out) {
  std::string_view value;
  const simdjson::error_code error = root[key].get(value);
  if (error == simdjson::NO_SUCH_FIELD) {
    return true;
  }
  if (error) {
    ALOGE("%.*s: \"%.*s\" must be a string", static_cast<int>(file_path.size()),
      file_path.data(), static_cast<int>(key.size()), key.data());
    return false;
  }
  out = value;
  return true;
}

}  // namespace

bool IsProjectFile(std::string_view path) {
  const std::string_view extension = GetFileExtension(path);
  if (extension.size() != kProjectFileExtension.size()) {
    return false;
  }
  for (size_t i = 0; i < extension.size(); ++i) {
    char c = extension[i];
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
    if (c != kProjectFileExtension[i]) {
      return false;
    }
  }
  return true;
}

std::optional<ProjectInfo> ParseProject(std::string_view json, std::string_view file_path) {
  if (json.starts_with(kUtf8Bom)) {
    json.remove_prefix(kUtf8Bom.size());
  }

  simdjson::dom::parser parser;
  simdjson::dom::object root;
  const simdjson::error_code error = parser.parse(json.data(), json.size()).get(root);
  if (error) {
    ALOGE("%.*s: invalid project file, %s", static_cast<int>(file_path.size()),
      file_path.data(), simdjson::error_message(error));
    return std::nullopt;
  }

  const fs::path file = ToNativePath(file_path);
  ProjectInfo info;
  info.name = ToUtf8(file.stem());
  info.file_path = std::string(file_path);
  info.root_path = ToUtf8(file.parent_path());
  if (!readString(root, "name", file_path, info.name) ||
      !readString(root, "engine_version", file_path, info.engine_version)) {
    return std::nullopt;
  }
  if (info.name.empty()) {
    ALOGE("%.*s: \"name\" is empty", static_cast<int>(file_path.size()), file_path.data());
    return std::nullopt;
  }
  return info;
}

std::optional<ProjectInfo> LoadProject(std::string_view path) {
  std::error_code error;
  fs::path absolute = fs::absolute(ToNativePath(path), error);
  if (!error) {
    absolute = fs::weakly_canonical(absolute, error);
  }
  if (error) {
    ALOGE("Invalid project path '%.*s'", static_cast<int>(path.size()), path.data());
    return std::nullopt;
  }

  const std::optional<fs::path> file = findProjectFile(absolute);
  if (!file) {
    return std::nullopt;
  }

  const std::string file_path = ToUtf8(*file);
  std::vector<uint8> bytes;
  if (!GetVirtualFileSystem().readFile(file_path, bytes)) {
    ALOGE("Cannot read '%s'", file_path.c_str());
    return std::nullopt;
  }
  return ParseProject(
    std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()),
    file_path);
}

void MountProject(const ProjectInfo& project) {
  UnmountProject();
  VirtualFileSystem& vfs = GetVirtualFileSystem();
  vfs.mount(kProjectScheme,
    std::make_unique<DirectorySource>(project.root_path + "/assets"));
  vfs.mount(kCacheScheme,
    std::make_unique<DirectorySource>(project.root_path + "/.amaryl/cache", true));
}

void UnmountProject() {
  VirtualFileSystem& vfs = GetVirtualFileSystem();
  vfs.unmount(kProjectScheme);
  vfs.unmount(kCacheScheme);
}

}  // namespace amaryl
