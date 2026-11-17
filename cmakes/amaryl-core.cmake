project(amaryl-core)
set(TARGET amaryl-core)

set(AMARYL_CORE_INCLUDE_ROOT ${AMARYL_INCLUDE_ROOT}/amaryl/core)
set(AMARYL_CORE_SOURCE_ROOT ${AMARYL_SOURCE_ROOT}/core)

set(AMARYL_CORE_SOURCES
  ${AMARYL_CORE_INCLUDE_ROOT}/amaryl.h
  ${AMARYL_CORE_INCLUDE_ROOT}/assert.h
  ${AMARYL_CORE_INCLUDE_ROOT}/atomic.h
  ${AMARYL_CORE_INCLUDE_ROOT}/atomic_cxx11.h
  ${AMARYL_CORE_INCLUDE_ROOT}/config.h
  ${AMARYL_CORE_INCLUDE_ROOT}/hash_combine.h
  ${AMARYL_CORE_INCLUDE_ROOT}/macros.h
  ${AMARYL_CORE_INCLUDE_ROOT}/murmur_hash.h
  ${AMARYL_CORE_INCLUDE_ROOT}/types.h

  ${AMARYL_CORE_INCLUDE_ROOT}/allocator/allocator.h
  ${AMARYL_CORE_INCLUDE_ROOT}/allocator/arena_allocator.h
  ${AMARYL_CORE_INCLUDE_ROOT}/allocator/bump_allocator.h
  ${AMARYL_CORE_INCLUDE_ROOT}/allocator/pool_allocator.h

  ${AMARYL_CORE_SOURCE_ROOT}/amaryl.cpp
  ${AMARYL_CORE_SOURCE_ROOT}/assert.cpp
  ${AMARYL_CORE_SOURCE_ROOT}/murmur_hash.cpp

  ${AMARYL_CORE_SOURCE_ROOT}/allocator/allocator.cpp
  ${AMARYL_CORE_SOURCE_ROOT}/allocator/arena_allocator.cpp
  ${AMARYL_CORE_SOURCE_ROOT}/allocator/bump_allocator.cpp
  ${AMARYL_CORE_SOURCE_ROOT}/allocator/pool_allocator.cpp
)

set(AMARYL_MATH_SOURCES
  ${AMARYL_CORE_INCLUDE_ROOT}/math/arm_neon.h
  ${AMARYL_CORE_INCLUDE_ROOT}/math/mat44.h
  ${AMARYL_CORE_INCLUDE_ROOT}/math/math.h
  ${AMARYL_CORE_INCLUDE_ROOT}/math/math_constants.h
  ${AMARYL_CORE_INCLUDE_ROOT}/math/math_types.h
  ${AMARYL_CORE_INCLUDE_ROOT}/math/math_utils.h
  ${AMARYL_CORE_INCLUDE_ROOT}/math/quat.h
  ${AMARYL_CORE_INCLUDE_ROOT}/math/rotator.h
  ${AMARYL_CORE_INCLUDE_ROOT}/math/trigonometry.h
  ${AMARYL_CORE_INCLUDE_ROOT}/math/uvec4.h
  ${AMARYL_CORE_INCLUDE_ROOT}/math/vec3.h
  ${AMARYL_CORE_INCLUDE_ROOT}/math/vec4.h
  ${AMARYL_CORE_INCLUDE_ROOT}/math/vector_aliases.h

  ${AMARYL_CORE_SOURCE_ROOT}/math/mat44.cpp
  ${AMARYL_CORE_SOURCE_ROOT}/math/quat.cpp
  ${AMARYL_CORE_SOURCE_ROOT}/math/rotator.cpp
  ${AMARYL_CORE_SOURCE_ROOT}/math/vec3.cpp
)

add_library(${TARGET} SHARED ${AMARYL_CORE_SOURCES} ${AMARYL_MATH_SOURCES})
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/include/amaryl/core")
generate_export_header(${TARGET}
  EXPORT_MACRO_NAME CORE_API
  EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/generated/include/amaryl/core/core_export.h"
)
target_include_directories(${TARGET}
  PUBLIC
  ${AMARYL_INCLUDE_ROOT}
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated/include>
)

target_link_libraries(${TARGET}
  PUBLIC
    mimalloc
    plog::plog
    "$<$<PLATFORM_ID:Windows>:dbghelp>"
)
