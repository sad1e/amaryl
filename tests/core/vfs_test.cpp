#include "amaryl/core/filesystem/vfs.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "amaryl/core/filesystem/path_utils.h"
#include "boost/ut.hpp"

using namespace amaryl;
using namespace boost::ut;

namespace {

// "测试", non-ASCII names must survive the UTF-8 <-> native conversions.
constexpr const char* kNonAscii = "\xE6\xB5\x8B\xE8\xAF\x95";

std::vector<uint8> toBytes(std::string_view text) {
  return std::vector<uint8>(text.begin(), text.end());
}

std::string toText(const std::vector<uint8>& bytes) {
  return std::string(bytes.begin(), bytes.end());
}

// A fresh directory in the temp directory, removed on destruction.
struct TempDir {
  TempDir() {
    path = std::filesystem::temp_directory_path() /
           ToNativePath(std::string("amaryl-vfs-test-") + kNonAscii);
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
  }
  ~TempDir() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }

  std::string utf8(std::string_view relative = {}) const {
    return relative.empty() ? ToUtf8(path) : ToUtf8(path / ToNativePath(relative));
  }

  std::filesystem::path path;
};

}  // namespace

int main() {
  "native path utf8 roundtrip"_test = [] {
    const std::string utf8 = std::string("dir/") + kNonAscii + ".txt";
    expect(ToUtf8(ToNativePath(utf8)) == utf8);
  };

  "split virtual path"_test = [] {
    const auto parts = SplitVirtualPath("engine://shaders/a.spv");
    expect(parts.has_value());
    expect(parts->scheme == "engine");
    expect(parts->relative == "shaders/a.spv");

    expect(IsVirtualPath("project://"));
    expect(!IsVirtualPath("C://Windows"));
    expect(!IsVirtualPath("C:\\Windows"));
    expect(!IsVirtualPath("Engine://a"));
    expect(!IsVirtualPath("://a"));
    expect(!IsVirtualPath("shaders/a.spv"));
  };

  "normalize relative path"_test = [] {
    expect(NormalizeRelativePath("a/b/c.txt") == std::optional<std::string>("a/b/c.txt"));
    expect(NormalizeRelativePath("\\a\\\\b/./c.txt") ==
           std::optional<std::string>("a/b/c.txt"));
    expect(NormalizeRelativePath("a/../b") == std::optional<std::string>("b"));
    expect(NormalizeRelativePath("") == std::optional<std::string>(""));
    expect(!NormalizeRelativePath("..").has_value());
    expect(!NormalizeRelativePath("a/../../b").has_value());
    expect(!NormalizeRelativePath("C:/Windows").has_value());
    expect(!NormalizeRelativePath("file.txt:stream").has_value());

    expect(NormalizeVirtualPath("engine://./a//b/../c") ==
           std::optional<std::string>("engine://a/c"));
    expect(!NormalizeVirtualPath("engine://../a").has_value());
    expect(!NormalizeVirtualPath("a/b").has_value());
  };

  "file name and extension"_test = [] {
    expect(GetFileName("engine://shaders/a.vert.spv") == "a.vert.spv");
    expect(GetFileName("C:\\dir\\b.png") == "b.png");
    expect(GetFileName("c") == "c");
    expect(GetFileExtension("engine://shaders/a.vert.spv") == ".spv");
    expect(GetFileExtension("dir.d/file").empty());
    expect(GetFileExtension(".gitignore").empty());
  };

  "read and write through a directory source"_test = [] {
    const TempDir dir;
    VirtualFileSystem vfs;
    expect(!vfs.isMounted("user"));
    expect(vfs.mount("user", std::make_unique<DirectorySource>(dir.utf8(), true)));
    expect(vfs.isMounted("user"));

    const std::string path = std::string("user://saves/") + kNonAscii + ".sav";
    expect(!vfs.exists(path));
    expect(vfs.writeFile(path, toBytes("hello")));
    expect(vfs.exists(path));
    expect(vfs.exists("user://saves/./../saves/" + std::string(kNonAscii) + ".sav"));

    std::vector<uint8> bytes;
    expect(vfs.readFile(path, bytes));
    expect(toText(bytes) == "hello");

    // The file is a loose file with the expected native name.
    const std::string native = vfs.getNativePath(path);
    expect(native == dir.utf8(std::string("saves/") + kNonAscii + ".sav"));
    expect(std::filesystem::is_regular_file(ToNativePath(native)));

    // Native paths bypass the mounts.
    bytes.clear();
    expect(vfs.readFile(native, bytes));
    expect(toText(bytes) == "hello");

    vfs.unmount("user");
    expect(!vfs.exists(path));
  };

  "read only sources reject writes"_test = [] {
    const TempDir dir;
    VirtualFileSystem vfs;
    expect(vfs.mount("engine", std::make_unique<DirectorySource>(dir.utf8())));
    expect(!vfs.writeFile("engine://a.txt", toBytes("a")));
    expect(!std::filesystem::exists(dir.path / "a.txt"));
  };

  "paths cannot escape the mount root"_test = [] {
    const TempDir dir;
    std::filesystem::create_directories(dir.path / "root");
    {
      VirtualFileSystem vfs;
      expect(vfs.mount("user", std::make_unique<DirectorySource>(dir.utf8(), true)));
      expect(vfs.writeFile("user://secret.txt", toBytes("secret")));
    }

    VirtualFileSystem vfs;
    expect(vfs.mount("user", std::make_unique<DirectorySource>(dir.utf8("root"), true)));
    std::vector<uint8> bytes;
    expect(!vfs.exists("user://../secret.txt"));
    expect(!vfs.readFile("user://../secret.txt", bytes));
    expect(!vfs.writeFile("user://../escaped.txt", toBytes("x")));
    expect(!std::filesystem::exists(dir.path / "escaped.txt"));
  };

  "higher priority and later mounts win"_test = [] {
    const TempDir dir;
    VirtualFileSystem vfs;
    expect(vfs.mount("base", std::make_unique<DirectorySource>(dir.utf8("base"), true)));
    expect(vfs.writeFile("base://a.txt", toBytes("base")));
    expect(vfs.writeFile("base://b.txt", toBytes("base")));
    expect(
      vfs.mount("patch", std::make_unique<DirectorySource>(dir.utf8("patch"), true)));
    expect(vfs.writeFile("patch://a.txt", toBytes("patch")));

    VirtualFileSystem game;
    expect(
      game.mount("project", std::make_unique<DirectorySource>(dir.utf8("patch")), 10));
    expect(game.mount("project", std::make_unique<DirectorySource>(dir.utf8("base"))));

    std::vector<uint8> bytes;
    expect(game.readFile("project://a.txt", bytes));
    expect(toText(bytes) == "patch");
    // Falls back to the lower priority source.
    expect(game.readFile("project://b.txt", bytes));
    expect(toText(bytes) == "base");
    expect(!game.readFile("project://c.txt", bytes));

    // Same priority: the latest mount is tried first.
    VirtualFileSystem latest;
    expect(latest.mount("project", std::make_unique<DirectorySource>(dir.utf8("base"))));
    expect(latest.mount("project", std::make_unique<DirectorySource>(dir.utf8("patch"))));
    expect(latest.readFile("project://a.txt", bytes));
    expect(toText(bytes) == "patch");
  };

  "invalid mounts are rejected"_test = [] {
    VirtualFileSystem vfs;
    expect(!vfs.mount("C", std::make_unique<DirectorySource>(".")));
    expect(!vfs.mount("Engine", std::make_unique<DirectorySource>(".")));
    expect(!vfs.mount("engine", nullptr));
    std::vector<uint8> bytes;
    expect(!vfs.readFile("engine://a.txt", bytes));
  };
}
