set(_LLVM_ROOT_CANDIDATES)

if(DEFINED ENV{SCOOP})
  list(APPEND _LLVM_ROOT_CANDIDATES "$ENV{SCOOP}/apps/llvm/current")
endif()

if(DEFINED ENV{SCOOP_GLOBAL})
  list(APPEND _LLVM_ROOT_CANDIDATES "$ENV{SCOOP_GLOBAL}/apps/llvm/current")
endif()

if(DEFINED ENV{HOME})
  list(APPEND _LLVM_ROOT_CANDIDATES "$ENV{HOME}/scoop/apps/llvm/current")
endif()

set(_LLVM_BIN "")

# Find Scoop installation.
foreach(_root IN LISTS _LLVM_ROOT_CANDIDATES)
  if(EXISTS "${_root}/bin/clang++.exe")
    set(_LLVM_BIN "${_root}/bin")
    break()
  endif()
endforeach()

# Fallback: Find LLVM from PATH.
if(NOT _LLVM_BIN)
  find_program(_CLANGXX_EXECUTABLE NAMES clang++.exe clang++)

  if(NOT _CLANGXX_EXECUTABLE)
    message(FATAL_ERROR "Cannot find clang++")
  endif()

  get_filename_component(_LLVM_BIN "${_CLANGXX_EXECUTABLE}" DIRECTORY)
endif()

# Compiler paths.
set(_CLANG_EXECUTABLE "${_LLVM_BIN}/clang.exe")
set(_CLANGXX_EXECUTABLE "${_LLVM_BIN}/clang++.exe")

if(NOT EXISTS "${_CLANG_EXECUTABLE}")
  message(FATAL_ERROR "Cannot find clang.exe")
endif()

if(NOT EXISTS "${_CLANGXX_EXECUTABLE}")
  message(FATAL_ERROR "Cannot find clang++.exe")
endif()

# Verify LLVM version
execute_process(
  COMMAND "${_CLANGXX_EXECUTABLE}" --version
  OUTPUT_VARIABLE _CLANG_VERSION
  RESULT_VARIABLE _CLANG_VERSION_RESULT
)

if(NOT _CLANG_VERSION_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to execute clang++")
endif()

# clang 23.1.5 will still link to
if(NOT _CLANG_VERSION MATCHES "clang version 23\\.1\\.5")
  message(FATAL_ERROR "Expected LLVM 23.1.5:\n${_CLANG_VERSION}")
endif()

message(STATUS "LLVM Compiler: ${_CLANG_VERSION}")

# Compiler
set(CMAKE_C_COMPILER "${_CLANG_EXECUTABLE}" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${_CLANGXX_EXECUTABLE}" CACHE FILEPATH "" FORCE)

# MSVC ABI, GNU-style clang++ frontend.
set(CMAKE_C_COMPILER_TARGET "x86_64-pc-windows-msvc" CACHE STRING "" FORCE)
set(CMAKE_CXX_COMPILER_TARGET "x86_64-pc-windows-msvc" CACHE STRING "" FORCE)

# LLVM tools
if(EXISTS "${_LLVM_BIN}/llvm-ar.exe")
  set(CMAKE_AR "${_LLVM_BIN}/llvm-ar.exe" CACHE FILEPATH "" FORCE)
endif()

if(EXISTS "${_LLVM_BIN}/llvm-ranlib.exe")
  set(CMAKE_RANLIB "${_LLVM_BIN}/llvm-ranlib.exe" CACHE FILEPATH "" FORCE)
endif()
