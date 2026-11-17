# plog
set(AMARYL_3RDPARTY_ROOT ${AMARYL_ROOT}/thirdparty)
add_subdirectory(${AMARYL_3RDPARTY_ROOT}/plog)

# mimalloc
set(MI_USE_CXX ON)
set(MI_BUILD_STATIC OFF)
set(MI_BUILD_SHARED ON)
add_subdirectory(${AMARYL_3RDPARTY_ROOT}/mimalloc)

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
