# plog
set(AMARYL_3RDPARTY_ROOT ${AMARYL_ROOT}/thirdparty)
add_subdirectory(${AMARYL_3RDPARTY_ROOT}/plog)
# `char` strings are UTF-8 everywhere. plog only detects it with MSVC's `/utf-8`, every
# target linking plog must agree since amaryl-core exports its implementation.
target_compile_definitions(plog INTERFACE PLOG_CHAR_IS_UTF8=1)

# mimalloc
set(MI_USE_CXX ON)
set(MI_BUILD_STATIC OFF)
set(MI_BUILD_SHARED ON)
add_subdirectory(${AMARYL_3RDPARTY_ROOT}/mimalloc)

function(amaryl_inject_mimalloc target)
  if(NOT WIN32)
    return()
  endif()
  set(_minject "${AMARYL_3RDPARTY_ROOT}/mimalloc/bin/minject.exe")
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_postfix "--postfix=debug") # debug: inject mimalloc-debug.dll
  else()
    set(_postfix "") # release: inject mimalloc.dll
  endif()
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${_minject}" --force --inplace ${_postfix} "$<TARGET_FILE:${target}>"
    COMMENT "Inject mimalloc as first import of ${target} (mimalloc-redirect)"
    VERBATIM)
endfunction()


# nanobench
add_subdirectory(${AMARYL_3RDPARTY_ROOT}/nanobench)

# SDL3
set(SDL_SHARED ON)
set(SDL_STATIC OFF)
set(SDL_TEST_LIBRARY OFF)
set(SDL_GPU OFF)
set(SDL_RENDER OFF)
set(SDL_CAMERA OFF)
set(SDL_TRAY OFF)
set(SDL_OPENGL OFF)
set(SDL_OPENGLES OFF)
add_subdirectory(${AMARYL_3RDPARTY_ROOT}/SDL3)
if(AMARYL_SUPPRESS_WARNINGS AND TARGET SDL3-shared)
  # Must be added after SDL's own `-Wall`, otherwise `-Wall` re-enables it
  target_compile_options(SDL3-shared PRIVATE
    $<$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>:-Wno-deprecated-declarations>
    $<$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>:-Wno-unused-but-set-global>
  )
endif()

# pystring
add_subdirectory(${AMARYL_3RDPARTY_ROOT}/pystring)

# cgltf
add_subdirectory(${AMARYL_3RDPARTY_ROOT}/cgltf)

# meshoptimizer
add_subdirectory(${AMARYL_3RDPARTY_ROOT}/meshoptimizer)

# simdjson
add_subdirectory(${AMARYL_3RDPARTY_ROOT}/simdjson)
