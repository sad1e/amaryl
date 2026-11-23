#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "amaryl/core/config.h"

namespace amaryl {

// Paths are UTF-8 `std::string` in the whole engine and are only converted at the OS
// boundary. Never build a `std::filesystem::path` from a `std::string` and never call
// `path::string()`: both use the ANSI code page on Windows, not UTF-8.

inline std::filesystem::path ToNativePath(std::string_view utf8) {
  return std::filesystem::path(
    std::u8string_view(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

// Keeps the native separators ('\' on Windows).
inline std::string ToUtf8(const std::filesystem::path& path) {
  const std::u8string utf8 = path.u8string();
  return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

// A virtual path is "scheme://relative/path", see `VirtualFileSystem`.
struct VirtualPathParts {
  std::string_view scheme;
  std::string_view relative;
};

// [a-z0-9_-]{2,}, two characters at least so a drive letter ("C://") is never a scheme.
CORE_API bool IsValidScheme(std::string_view scheme);
CORE_API bool IsVirtualPath(std::string_view path);
// Returns nullopt for native paths. The relative part is not normalized.
CORE_API std::optional<VirtualPathParts> SplitVirtualPath(std::string_view path);

// Converts '\' to '/', drops the empty and "." segments and resolves "..". Returns
// nullopt when ".." escapes the root or a segment contains ':' (drive or stream), so the
// result can never leave the directory it is appended to. A leading '/' is ignored.
CORE_API std::optional<std::string> NormalizeRelativePath(std::string_view path);
// "scheme://normalized/relative", nullopt when `path` is not a valid virtual path.
CORE_API std::optional<std::string> NormalizeVirtualPath(std::string_view path);

// Last segment of a virtual or native path.
CORE_API std::string_view GetFileName(std::string_view path);
// Extension of the file name with its dot (".png"), empty when there is none. A leading
// dot is part of the name (".gitignore" has no extension).
CORE_API std::string_view GetFileExtension(std::string_view path);

}  // namespace amaryl
