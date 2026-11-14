cmake_minimum_required(VERSION 3.20.0)
project(amaryl VERSION 0.0.1
  DESCRIPTION "amaryl"
  LANGUAGES CXX)

include(GenerateExportHeader)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "") # works

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(AMARYL_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND CMAKE_MODULE_PATH "${AMARYL_ROOT}/cmakes")

set(AMARYL_INCLUDE_ROOT ${AMARYL_ROOT}/include)
set(AMARYL_SOURCE_ROOT ${AMARYL_ROOT}/src)

option(AMARYL_SUPPRESS_WARNINGS "Suppress selected compiler warnings" OFF)
if(AMARYL_SUPPRESS_WARNINGS)
  add_compile_options(
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wno-deprecated-declarations>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wno-nullability-completeness>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wno-unused-but-set-global>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wno-nontrivial-memcall>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wno-delete-incomplete>
  )
endif()

# The `AMARYL_PRESET_TYPE` defined in CMakePresets.json
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${AMARYL_ROOT}/out/${AMARYL_PRESET_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${AMARYL_ROOT}/out/${AMARYL_PRESET_TYPE})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${AMARYL_ROOT}/out/${AMARYL_PRESET_TYPE})

include(amaryl-core)
if(AMARYL_ENABLE_TESTS)
  include(amaryl-tests)
endif()
