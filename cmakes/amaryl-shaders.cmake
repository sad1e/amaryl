# Compiles the project's Slang shaders into SPIR-V with slangc.
#
# Each `.slang` file is compiled once per entry point (`vertexMain` / `fragmentMain`)
# into a separate `.spv` module. The SPIR-V entry point names are preserved with
# `-fvk-use-entrypoint-name` so they match the names declared in
# `amaryl/nrhi/definitions.h` (`kVertexEntryName`, `kFragmentEntryName`).
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

# Accumulates every generated `.spv` file so they can be driven by a single target.
set(AMARYL_SHADER_SPV_FILES "")

macro(amaryl_compile_slang_shader slang_file)
  get_filename_component(_name "${slang_file}" NAME_WE)

  list(LENGTH AMARYL_SHADER_ENTRY_POINTS _num_entries)
  math(EXPR _last_index "${_num_entries} - 1")

  foreach(_i RANGE 0 ${_last_index})
    list(GET AMARYL_SHADER_ENTRY_POINTS ${_i} _entry)
    list(GET AMARYL_SHADER_STAGES ${_i} _stage)
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
              "${AMARYL_INCLUDE_ROOT}/amaryl/nrhi/descriptor.shared.h"
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
