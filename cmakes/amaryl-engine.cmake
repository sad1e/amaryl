project(amaryl-engine)
set(TARGET amaryl-engine)

set(AMARYL_ENGINE_INCLUDE_ROOT ${AMARYL_INCLUDE_ROOT}/amaryl/engine)
set(AMARYL_ENGINE_SOURCE_ROOT ${AMARYL_SOURCE_ROOT}/engine)

set(AMARYL_ENGINE_SOURCES
  ${AMARYL_ENGINE_INCLUDE_ROOT}/primitive.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/renderer.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/scene_graph.h

  ${AMARYL_ENGINE_SOURCE_ROOT}/renderer.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/scene_graph.cpp
)

add_library(${TARGET} SHARED ${AMARYL_ENGINE_SOURCES})
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/include/amaryl/engine")
generate_export_header(${TARGET}
  EXPORT_MACRO_NAME ENGINE_API
  EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/generated/include/amaryl/engine/engine_export.h"
)
target_include_directories(${TARGET}
  PUBLIC
  ${AMARYL_INCLUDE_ROOT}
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated/include>
)

target_link_libraries(${TARGET}
  PUBLIC
  amaryl-core
  amaryl-nrhi
  plog::plog
  pystring
  SDL3::SDL3)

