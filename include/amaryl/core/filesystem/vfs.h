#pragma once

#include <filesystem>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#include "amaryl/core/macros.h"
#include "amaryl/core/span.h"

namespace amaryl {

// Mount points used by the engine. The executable decides where they point to:
// - engine:  engine content, `<exe>/engine/` (shaders, default resources).
// - project: project assets, `<project>/assets/` in the editor, a pak once packaged.
// - cache:   imported data, `<project>/.amaryl/cache/`, editor only.
// - user:    writable data (saves, settings, logs).
inline constexpr std::string_view kEngineScheme = "engine";
inline constexpr std::string_view kProjectScheme = "project";
inline constexpr std::string_view kCacheScheme = "cache";
inline constexpr std::string_view kUserScheme = "user";

// Backend of a mount point. `relative_path` is always normalized by the file system
// ("dir/file.ext", '/' separators, no "." or ".." segments), see NormalizeRelativePath.
struct CORE_API IMountSource {
  virtual ~IMountSource() = default;

  virtual bool exists(std::string_view relative_path) const = 0;
  virtual bool readFile(
    std::string_view relative_path, std::vector<uint8>& out) const = 0;

  virtual bool isWritable() const { return false; }
  virtual bool writeFile(std::string_view relative_path, Span<const uint8> data) {
    UNREF_PARAM(relative_path);
    UNREF_PARAM(data);
    return false;
  }

  // UTF-8 path on disk, empty when the source has no loose files (e.g. a pak).
  virtual std::string getNativePath(std::string_view relative_path) const {
    UNREF_PARAM(relative_path);
    return {};
  }
};

// Loose files of a directory on disk.
struct CORE_API DirectorySource final : IMountSource {
  // `root` is a native UTF-8 path, it is created when the first file is written.
  explicit DirectorySource(std::string_view root, bool writable = false);

  bool exists(std::string_view relative_path) const override;
  bool readFile(std::string_view relative_path, std::vector<uint8>& out) const override;
  bool isWritable() const override { return writable_; }
  bool writeFile(std::string_view relative_path, Span<const uint8> data) override;
  std::string getNativePath(std::string_view relative_path) const override;

 private:
  std::filesystem::path resolve(std::string_view relative_path) const;

  std::filesystem::path root_;
  bool writable_{false};
};

// Maps "scheme://relative/path" to mount sources so the engine never depends on where
// the files are: loose directories in the editor, paks in a packaged game.
//
// A path without scheme is a native UTF-8 path and goes straight to the disk, it is meant
// for tools and for importing external files. Game code should only use virtual paths.
//
// Several sources can share a scheme (e.g. a patch over the base content): the highest
// priority is tried first, then the latest mounted. The mounts are usually set once at
// startup, the file operations are thread safe.
struct CORE_API VirtualFileSystem {
  VirtualFileSystem() = default;
  ~VirtualFileSystem() = default;
  NO_COPYABLE_AND_MOVEABLE(VirtualFileSystem);

  bool mount(
    std::string_view scheme, std::unique_ptr<IMountSource> source, int32 priority = 0);
  // Removes every source of `scheme`.
  void unmount(std::string_view scheme);
  void unmountAll();
  bool isMounted(std::string_view scheme) const;

  bool exists(std::string_view path) const;
  // Reads the file from the first source that has it.
  bool readFile(std::string_view path, std::vector<uint8>& out) const;
  // Writes to the first writable source of the scheme, the parent directories are
  // created.
  bool writeFile(std::string_view path, Span<const uint8> data);
  // Native UTF-8 path of the file in the first source that has it, empty when the file
  // does not exist or is not a loose file. Editor and tools only.
  std::string getNativePath(std::string_view path) const;

 private:
  struct Mount {
    std::string scheme;
    int32 priority{0};
    uint64 order{0};
    std::unique_ptr<IMountSource> source;
  };

  mutable std::shared_mutex mutex_;
  // Sorted by lookup order.
  std::vector<Mount> mounts_;
  uint64 next_order_{0};
};

// The file system shared by every module.
CORE_API VirtualFileSystem& GetVirtualFileSystem();

}  // namespace amaryl
