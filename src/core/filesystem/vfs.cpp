#include "amaryl/core/filesystem/vfs.h"

#include <algorithm>
#include <fstream>
#include <mutex>
#include <optional>

#include "amaryl/core/filesystem/path_utils.h"
#include "amaryl/core/logger.h"

namespace amaryl {

namespace {

bool isRegularFile(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::is_regular_file(path, error);
}

bool readNativeFile(const std::filesystem::path& path, std::vector<uint8>& out) {
  if (!isRegularFile(path)) {
    return false;
  }
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return false;
  }
  const std::streamoff size = file.tellg();
  if (size < 0) {
    return false;
  }
  std::vector<uint8> bytes(static_cast<size_t>(size));
  file.seekg(0, std::ios::beg);
  file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
  if (file.fail()) {
    return false;
  }
  out = std::move(bytes);
  return true;
}

bool writeNativeFile(const std::filesystem::path& path, Span<const uint8> data) {
  std::error_code error;
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
      return false;
    }
  }
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    return false;
  }
  file.write(reinterpret_cast<const char*>(data.data()),
    static_cast<std::streamsize>(data.size()));
  file.close();
  return !file.fail();
}

struct ResolvedPath {
  std::string_view scheme;
  std::string relative;
};

enum class EPathKind { Virtual, Native, Invalid };

// Splits and normalizes `path`, `out` is only filled for virtual paths.
EPathKind resolvePath(std::string_view path, ResolvedPath& out) {
  const std::optional<VirtualPathParts> parts = SplitVirtualPath(path);
  if (!parts) {
    return EPathKind::Native;
  }
  std::optional<std::string> relative = NormalizeRelativePath(parts->relative);
  if (!relative) {
    ALOGW("VFS: invalid path '%.*s'", static_cast<int>(path.size()), path.data());
    return EPathKind::Invalid;
  }
  out.scheme = parts->scheme;
  out.relative = std::move(*relative);
  return EPathKind::Virtual;
}

}  // namespace

DirectorySource::DirectorySource(std::string_view root, bool writable)
    : root_(ToNativePath(root)), writable_(writable) {}

std::filesystem::path DirectorySource::resolve(std::string_view relative_path) const {
  return root_ / ToNativePath(relative_path);
}

bool DirectorySource::exists(std::string_view relative_path) const {
  return isRegularFile(resolve(relative_path));
}

bool DirectorySource::readFile(
  std::string_view relative_path, std::vector<uint8>& out) const {
  return readNativeFile(resolve(relative_path), out);
}

bool DirectorySource::writeFile(std::string_view relative_path, Span<const uint8> data) {
  return writable_ && !relative_path.empty() &&
         writeNativeFile(resolve(relative_path), data);
}

std::string DirectorySource::getNativePath(std::string_view relative_path) const {
  return ToUtf8(resolve(relative_path));
}

bool VirtualFileSystem::mount(
  std::string_view scheme, std::unique_ptr<IMountSource> source, int32 priority) {
  if (!source || !IsValidScheme(scheme)) {
    ALOGE("VFS: cannot mount '%.*s'", static_cast<int>(scheme.size()), scheme.data());
    return false;
  }

  std::unique_lock lock(mutex_);
  mounts_.push_back({std::string(scheme), priority, next_order_++, std::move(source)});
  std::sort(mounts_.begin(), mounts_.end(), [](const Mount& a, const Mount& b) {
    return a.priority != b.priority ? a.priority > b.priority : a.order > b.order;
  });
  return true;
}

void VirtualFileSystem::unmount(std::string_view scheme) {
  std::unique_lock lock(mutex_);
  std::erase_if(mounts_, [&](const Mount& mount) { return mount.scheme == scheme; });
}

void VirtualFileSystem::unmountAll() {
  std::unique_lock lock(mutex_);
  mounts_.clear();
}

bool VirtualFileSystem::isMounted(std::string_view scheme) const {
  std::shared_lock lock(mutex_);
  return std::any_of(mounts_.begin(), mounts_.end(),
    [&](const Mount& mount) { return mount.scheme == scheme; });
}

bool VirtualFileSystem::exists(std::string_view path) const {
  ResolvedPath resolved;
  switch (resolvePath(path, resolved)) {
    case EPathKind::Native:
      return isRegularFile(ToNativePath(path));
    case EPathKind::Invalid:
      return false;
    case EPathKind::Virtual:
      break;
  }

  std::shared_lock lock(mutex_);
  for (const Mount& mount : mounts_) {
    if (mount.scheme == resolved.scheme && mount.source->exists(resolved.relative)) {
      return true;
    }
  }
  return false;
}

bool VirtualFileSystem::readFile(std::string_view path, std::vector<uint8>& out) const {
  ResolvedPath resolved;
  switch (resolvePath(path, resolved)) {
    case EPathKind::Native:
      return readNativeFile(ToNativePath(path), out);
    case EPathKind::Invalid:
      return false;
    case EPathKind::Virtual:
      break;
  }

  std::shared_lock lock(mutex_);
  for (const Mount& mount : mounts_) {
    if (mount.scheme == resolved.scheme &&
        mount.source->readFile(resolved.relative, out)) {
      return true;
    }
  }
  return false;
}

bool VirtualFileSystem::writeFile(std::string_view path, Span<const uint8> data) {
  ResolvedPath resolved;
  switch (resolvePath(path, resolved)) {
    case EPathKind::Native:
      return writeNativeFile(ToNativePath(path), data);
    case EPathKind::Invalid:
      return false;
    case EPathKind::Virtual:
      break;
  }

  std::shared_lock lock(mutex_);
  for (const Mount& mount : mounts_) {
    if (mount.scheme == resolved.scheme && mount.source->isWritable()) {
      return mount.source->writeFile(resolved.relative, data);
    }
  }
  return false;
}

std::string VirtualFileSystem::getNativePath(std::string_view path) const {
  ResolvedPath resolved;
  switch (resolvePath(path, resolved)) {
    case EPathKind::Native:
      return isRegularFile(ToNativePath(path)) ? std::string(path) : std::string();
    case EPathKind::Invalid:
      return {};
    case EPathKind::Virtual:
      break;
  }

  std::shared_lock lock(mutex_);
  for (const Mount& mount : mounts_) {
    if (mount.scheme == resolved.scheme && mount.source->exists(resolved.relative)) {
      // The file may be in a pak, a lower priority loose copy would be stale.
      return mount.source->getNativePath(resolved.relative);
    }
  }
  return {};
}

VirtualFileSystem& GetVirtualFileSystem() {
  static VirtualFileSystem vfs;
  return vfs;
}

}  // namespace amaryl
