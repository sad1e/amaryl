project(amaryl-app)
set(TARGET_EDITOR amaryl-editor)

set(AMARYL_APP_INCLUDE_ROOT ${AMARYL_INCLUDE_ROOT}/amaryl/app)
set(AMARYL_APP_SOURCE_ROOT ${AMARYL_SOURCE_ROOT}/app)

# Embeds the application manifest (UTF-8 active code page) into a Windows executable.
function(amaryl_add_windows_manifest target)
  if(WIN32)
    target_sources(${target} PRIVATE ${AMARYL_SOURCE_ROOT}/app/amaryl.manifest)
  endif()
endfunction()

set(AMARYL_APP_SOURCES
  ${AMARYL_APP_INCLUDE_ROOT}/app.h
  ${AMARYL_APP_SOURCE_ROOT}/app.cpp
)

add_executable(${TARGET_EDITOR}
  ${AMARYL_APP_INCLUDE_ROOT}/editor.h
  ${AMARYL_APP_SOURCE_ROOT}/editor.cpp
  ${AMARYL_APP_SOURCES})

target_include_directories(${TARGET_EDITOR}
  PUBLIC
  ${AMARYL_INCLUDE_ROOT})
target_link_libraries(${TARGET_EDITOR}
  PUBLIC
  amaryl-core
  amaryl-engine
  amaryl-nrhi
  SDL3::SDL3
  plog::plog)
target_compile_definitions(${TARGET_EDITOR}
  PUBLIC
  SDL_MAIN_USE_CALLBACKS)
amaryl_add_windows_manifest(${TARGET_EDITOR})
amaryl_inject_mimalloc(${TARGET_EDITOR})

# Compiled Slang shaders must be present in `engine/shaders/` next to the executable.
if(TARGET amaryl-shaders)
  add_dependencies(${TARGET_EDITOR} amaryl-shaders)
endif()
