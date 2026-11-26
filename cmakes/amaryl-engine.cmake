project(amaryl-engine)
set(TARGET amaryl-engine)

set(AMARYL_ENGINE_INCLUDE_ROOT ${AMARYL_INCLUDE_ROOT}/amaryl/engine)
set(AMARYL_ENGINE_SOURCE_ROOT ${AMARYL_SOURCE_ROOT}/engine)

set(AMARYL_ENGINE_SOURCES
  ${AMARYL_ENGINE_INCLUDE_ROOT}/common.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/engine.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/input_system.h

  ${AMARYL_ENGINE_SOURCE_ROOT}/engine.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/input_system.cpp

  ${AMARYL_ENGINE_INCLUDE_ROOT}/asset/asset_importer.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/asset/asset_manager.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/asset/image_decoder.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/asset/material_asset.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/asset/mesh_asset.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/asset/scene_asset.h

  ${AMARYL_ENGINE_SOURCE_ROOT}/asset/asset_importer.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/asset/image_decoder.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/asset/mesh_asset.cpp

  ${AMARYL_ENGINE_INCLUDE_ROOT}/project/project.h

  ${AMARYL_ENGINE_SOURCE_ROOT}/project/project.cpp

  ${AMARYL_ENGINE_INCLUDE_ROOT}/render/gpu_scene.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/render/gpu_scene.shared.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/render/gpu_scene_buffers.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/render/material_registry.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/render/mesh_registry.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/render/range_allocator.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/render/render_scene.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/render/deferred_renderer.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/render/imgui_renderer.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/render/shader_loader.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/render/texture.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/render/texture_registry.h

  ${AMARYL_ENGINE_SOURCE_ROOT}/render/gpu_scene_buffers.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/render/material_registry.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/render/mesh_registry.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/render/range_allocator.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/render/render_scene.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/render/deferred_renderer.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/render/imgui_renderer.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/render/scene_render_targets.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/render/shader_loader.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/render/texture.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/render/texture_registry.cpp

  ${AMARYL_ENGINE_INCLUDE_ROOT}/scene/camera.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/scene/common.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/scene/entity.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/scene/light.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/scene/model.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/scene/model_library.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/scene/scene_graph.h
  ${AMARYL_ENGINE_INCLUDE_ROOT}/scene/scene_loader.h

  ${AMARYL_ENGINE_SOURCE_ROOT}/scene/camera.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/scene/entity.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/scene/light.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/scene/model.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/scene/model_library.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/scene/scene_graph.cpp
  ${AMARYL_ENGINE_SOURCE_ROOT}/scene/scene_loader.cpp
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
  SDL3::SDL3
  cgltf
  imgui
  PRIVATE
  meshoptimizer
  simdjson
  stb)

# Editor-only data changes the layout of engine types (e.g. `Asset`), so the definition
# is PUBLIC: the engine and every target using its headers must agree.
option(AMARYL_EDITOR "Build the engine with the editor-only data" ON)
target_compile_definitions(${TARGET}
  PUBLIC
  AMARYL_EDITOR=$<BOOL:${AMARYL_EDITOR}>)
