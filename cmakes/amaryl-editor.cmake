project(amaryl-editor-core)
set(TARGET amaryl-editor-core)

set(AMARYL_EDITOR_INCLUDE_ROOT ${AMARYL_INCLUDE_ROOT}/amaryl/editor)
set(AMARYL_EDITOR_SOURCE_ROOT ${AMARYL_SOURCE_ROOT}/editor)

# Editor logic without UI, shared by the editor application and the tests.
set(AMARYL_EDITOR_SOURCES
  ${AMARYL_EDITOR_INCLUDE_ROOT}/common.h

  ${AMARYL_EDITOR_INCLUDE_ROOT}/model/signal.h
  ${AMARYL_EDITOR_INCLUDE_ROOT}/model/property.h
  ${AMARYL_EDITOR_INCLUDE_ROOT}/model/object_registry.h
  ${AMARYL_EDITOR_INCLUDE_ROOT}/model/transaction.h
  ${AMARYL_EDITOR_INCLUDE_ROOT}/model/history.h
  ${AMARYL_EDITOR_INCLUDE_ROOT}/model/edit_scope.h
  ${AMARYL_EDITOR_INCLUDE_ROOT}/model/selection.h
  ${AMARYL_EDITOR_INCLUDE_ROOT}/scene/editor_scene.h
  ${AMARYL_EDITOR_INCLUDE_ROOT}/scene/entity_type.h

  ${AMARYL_EDITOR_SOURCE_ROOT}/model/property.cpp
  ${AMARYL_EDITOR_SOURCE_ROOT}/model/object_registry.cpp
  ${AMARYL_EDITOR_SOURCE_ROOT}/model/history.cpp
  ${AMARYL_EDITOR_SOURCE_ROOT}/model/edit_scope.cpp
  ${AMARYL_EDITOR_SOURCE_ROOT}/model/selection.cpp
  ${AMARYL_EDITOR_SOURCE_ROOT}/scene/editor_scene.cpp
  ${AMARYL_EDITOR_SOURCE_ROOT}/scene/entity_type.cpp
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

# ImGui editor framework: shell, contexts, panels and actions.
set(TARGET_UI amaryl-editor-ui)
set(AMARYL_EDITOR_UI_SOURCES
  ${AMARYL_EDITOR_INCLUDE_ROOT}/framework/common.h
  ${AMARYL_EDITOR_INCLUDE_ROOT}/framework/action.h
  ${AMARYL_EDITOR_INCLUDE_ROOT}/framework/panel.h
  ${AMARYL_EDITOR_INCLUDE_ROOT}/framework/editor_context.h
  ${AMARYL_EDITOR_INCLUDE_ROOT}/framework/editor.h
  ${AMARYL_EDITOR_INCLUDE_ROOT}/framework/editor_layer.h
  ${AMARYL_EDITOR_INCLUDE_ROOT}/panels/inspector_panel.h
  ${AMARYL_EDITOR_INCLUDE_ROOT}/panels/history_panel.h
  ${AMARYL_EDITOR_INCLUDE_ROOT}/level/level_context.h
  ${AMARYL_EDITOR_INCLUDE_ROOT}/level/outliner_panel.h

  ${AMARYL_EDITOR_SOURCE_ROOT}/framework/action.cpp
  ${AMARYL_EDITOR_SOURCE_ROOT}/framework/panel.cpp
  ${AMARYL_EDITOR_SOURCE_ROOT}/framework/editor_context.cpp
  ${AMARYL_EDITOR_SOURCE_ROOT}/framework/editor.cpp
  ${AMARYL_EDITOR_SOURCE_ROOT}/framework/editor_layer.cpp
  ${AMARYL_EDITOR_SOURCE_ROOT}/panels/inspector_panel.cpp
  ${AMARYL_EDITOR_SOURCE_ROOT}/panels/history_panel.cpp
  ${AMARYL_EDITOR_SOURCE_ROOT}/level/level_context.cpp
  ${AMARYL_EDITOR_SOURCE_ROOT}/level/outliner_panel.cpp

  ${AMARYL_3RDPARTY_ROOT}/imgui/backends/imgui_impl_sdl3.h
  ${AMARYL_3RDPARTY_ROOT}/imgui/backends/imgui_impl_sdl3.cpp
)

add_library(${TARGET_UI} SHARED ${AMARYL_EDITOR_UI_SOURCES})
generate_export_header(${TARGET_UI}
  EXPORT_MACRO_NAME EDITOR_UI_API
  EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/generated/include/amaryl/editor/editor_ui_export.h"
)
target_include_directories(${TARGET_UI}
  PUBLIC
  ${AMARYL_INCLUDE_ROOT}
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated/include>
  PRIVATE
  ${AMARYL_3RDPARTY_ROOT}/imgui/backends)
target_link_libraries(${TARGET_UI}
  PUBLIC
  amaryl-editor-core
  imgui
  SDL3::SDL3)
# imgui_impl_sdl3 is compiled into the editor UI: IMGUI_IMPL_API must not inherit the
# dllimport of IMGUI_API.
target_compile_definitions(${TARGET_UI}
  PRIVATE
  "IMGUI_IMPL_API=")
