set(AMARYL_3RDPARTY_ROOT ${AMARYL_ROOT}/thirdparty)
add_subdirectory(${AMARYL_3RDPARTY_ROOT}/plog)

set(MI_USE_CXX ON)
set(MI_BUILD_STATIC OFF)
set(MI_BUILD_SHARED ON)
add_subdirectory(${AMARYL_3RDPARTY_ROOT}/mimalloc)
