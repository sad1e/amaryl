# Compiles the project's Slang shaders into SPIR-V with slangc.
#
# Each `.slang` file is compiled once per entry point into a separate `.spv` module:
#   - `<name>.compute.slang`: `computeMain` -> `<name>.compute.spv`
#   - other files: `vertexMain` / `fragmentMain` -> `<name>.vertex.spv` / `.fragment.spv`
# The SPIR-V entry point names are preserved with `-fvk-use-entrypoint-name` so they
# match the names declared in `amaryl/nrhi/definitions.h` (`kVertexEntryName`, ...).
#
# The compiled modules are written to the engine content directory next to the runtime
# binaries (`out/<preset>/engine/shaders/`) and loaded through the virtual file system as
# `engine://shaders/<name>.<stage>.spv` (see `DeferredRenderer`).

set(AMARYL_SHADER_SOURCE_DIR ${AMARYL_ROOT}/shaders)
set(AMARYL_SHADER_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/engine/shaders)
file(MAKE_DIRECTORY ${AMARYL_SHADER_OUTPUT_DIR})

# Entry point name -> SPIR-V stage suffix. Order must be kept in sync.
set(AMARYL_SHADER_ENTRY_POINTS vertexMain fragmentMain)
set(AMARYL_SHADER_STAGES vertex fragment)
set(AMARYL_COMPUTE_SHADER_ENTRY_POINTS computeMain)
set(AMARYL_COMPUTE_SHADER_STAGES compute)

# Headers shared between C++ and Slang, every shader is rebuilt when they change.
set(AMARYL_SHADER_SHARED_HEADERS
  "${AMARYL_INCLUDE_ROOT}/amaryl/nrhi/descriptor.shared.h"
  "${AMARYL_INCLUDE_ROOT}/amaryl/engine/render/gpu_scene.shared.h")

# Accumulates every generated `.spv` file so they can be driven by a single target.
set(AMARYL_SHADER_SPV_FILES "")

macro(amaryl_compile_slang_shader slang_file)
  # NAME_WE strips every extension: "gpu-scene-scatter.compute.slang" -> "gpu-scene-scatter"
  get_filename_component(_name "${slang_file}" NAME_WE)
  if("${slang_file}" MATCHES "\\.compute\\.slang$")
    set(_entries ${AMARYL_COMPUTE_SHADER_ENTRY_POINTS})
    set(_stages ${AMARYL_COMPUTE_SHADER_STAGES})
  else()
    set(_entries ${AMARYL_SHADER_ENTRY_POINTS})
    set(_stages ${AMARYL_SHADER_STAGES})
  endif()

  list(LENGTH _entries _num_entries)
  math(EXPR _last_index "${_num_entries} - 1")

  foreach(_i RANGE 0 ${_last_index})
    list(GET _entries ${_i} _entry)
    list(GET _stages ${_i} _stage)
    set(_out "${AMARYL_SHADER_OUTPUT_DIR}/${_name}.${_stage}.spv")

    add_custom_command(
      OUTPUT "${_out}"
      COMMAND "${AMARYL_SLANGC_EXECUTABLE}" "${slang_file}"
              -target spirv
              -profile glsl_450
              -entry ${_entry}
              -stage ${_stage}
              -fvk-use-entrypoint-name
              -I "${AMARYL_INCLUDE_ROOT}"
              -o "${_out}"
      DEPENDS "${slang_file}"
              ${AMARYL_SHADER_SHARED_HEADERS}
      COMMENT "slangc: ${_name}.${_stage}.spv"
      VERBATIM)

    list(APPEND AMARYL_SHADER_SPV_FILES "${_out}")
  endforeach()
endmacro()

file(GLOB AMARYL_SLANG_FILES "${AMARYL_SHADER_SOURCE_DIR}/*.slang")
foreach(_slang_file ${AMARYL_SLANG_FILES})
  amaryl_compile_slang_shader("${_slang_file}")
endforeach()

if(AMARYL_SHADER_SPV_FILES)
  add_custom_target(amaryl-shaders ALL DEPENDS ${AMARYL_SHADER_SPV_FILES})
  set_target_properties(amaryl-shaders PROPERTIES FOLDER "shaders")
endif()
