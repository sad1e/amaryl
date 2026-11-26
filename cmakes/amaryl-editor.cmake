project(amaryl-editor-core)
set(TARGET amaryl-editor-core)

set(AMARYL_EDITOR_INCLUDE_ROOT ${AMARYL_INCLUDE_ROOT}/amaryl/editor)
set(AMARYL_EDITOR_SOURCE_ROOT ${AMARYL_SOURCE_ROOT}/editor)

# Editor logic without UI, shared by the editor application and the tests.
set(AMARYL_EDITOR_SOURCES
  ${AMARYL_EDITOR_INCLUDE_ROOT}/common.h

  ${AMARYL_EDITOR_INCLUDE_ROOT}/scene/editor_scene.h

  ${AMARYL_EDITOR_SOURCE_ROOT}/scene/editor_scene.cpp
)

add_library(${TARGET} SHARED ${AMARYL_EDITOR_SOURCES})
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/include/amaryl/editor")
generate_export_header(${TARGET}
  EXPORT_MACRO_NAME EDITOR_API
  EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/generated/include/amaryl/editor/editor_export.h"
)
target_include_directories(${TARGET}
  PUBLIC
  ${AMARYL_INCLUDE_ROOT}
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated/include>
)
target_link_libraries(${TARGET}
  PUBLIC
  amaryl-engine)
