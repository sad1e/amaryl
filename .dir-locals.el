((nil . ((my/project-commands
          . (("build debug"   . "cmake --build --preset clang-x64-debug")
             ("build release" . "cmake --build --preset clang-x64-release")
             ("configure"     . "cmake --preset clang-x64-debug")
             ("test"          . "ctest --preset clang-x64-debug")
             ("run"           . "build/clang-x64-debug/amaryl-editor.exe"))))))
