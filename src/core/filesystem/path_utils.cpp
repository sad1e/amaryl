#include "amaryl/core/filesystem/path_utils.h"

#include <vector>

namespace amaryl {

namespace {

constexpr std::string_view kSchemeSeparator = "://";

bool isSchemeChar(char c) {
  return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

}  // namespace

bool IsValidScheme(std::string_view scheme) {
  if (scheme.size() < 2) {
    return false;
  }
  for (const char c : scheme) {
    if (!isSchemeChar(c)) {
      return false;
    }
  }
  return true;
}

bool IsVirtualPath(std::string_view path) { return SplitVirtualPath(path).has_value(); }

std::optional<VirtualPathParts> SplitVirtualPath(std::string_view path) {
  const size_t separator = path.find(kSchemeSeparator);
  if (separator == std::string_view::npos) {
    return std::nullopt;
  }
  const std::string_view scheme = path.substr(0, separator);
  if (!IsValidScheme(scheme)) {
    return std::nullopt;
  }
  return VirtualPathParts{scheme, path.substr(separator + kSchemeSeparator.size())};
}

std::optional<std::string> NormalizeRelativePath(std::string_view path) {
  std::vector<std::string_view> segments;
  size_t start = 0;
  while (start <= path.size()) {
    size_t end = path.find_first_of("/\\", start);
    if (end == std::string_view::npos) {
      end = path.size();
    }
    const std::string_view segment = path.substr(start, end - start);
    start = end + 1;

    if (segment.empty() || segment == ".") {
      continue;
    }
    if (segment == "..") {
      if (segments.empty()) {
        return std::nullopt;
      }
      segments.pop_back();
      continue;
    }
    if (segment.find(':') != std::string_view::npos ||
        segment.find('\0') != std::string_view::npos) {
      return std::nullopt;
    }
    segments.push_back(segment);
  }

  std::string out;
  for (const std::string_view segment : segments) {
    if (!out.empty()) {
      out += '/';
    }
    out += segment;
  }
  return out;
}

std::optional<std::string> NormalizeVirtualPath(std::string_view path) {
  const std::optional<VirtualPathParts> parts = SplitVirtualPath(path);
  if (!parts) {
    return std::nullopt;
  }
  std::optional<std::string> relative = NormalizeRelativePath(parts->relative);
  if (!relative) {
    return std::nullopt;
  }
  std::string out;
  out.reserve(parts->scheme.size() + kSchemeSeparator.size() + relative->size());
  out += parts->scheme;
  out += kSchemeSeparator;
  out += *relative;
  return out;
}

std::string_view GetFileName(std::string_view path) {
  const size_t slash = path.find_last_of("/\\");
  return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

std::string_view GetFileExtension(std::string_view path) {
  const std::string_view name = GetFileName(path);
  const size_t dot = name.find_last_of('.');
  if (dot == std::string_view::npos || dot == 0) {
    return {};
  }
  return name.substr(dot);
}

}  // namespace amaryl
