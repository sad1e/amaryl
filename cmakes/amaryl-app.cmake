project(amaryl-app)
set(TARGET_APP amaryl-app)

set(AMARYL_APP_INCLUDE_ROOT ${AMARYL_INCLUDE_ROOT}/amaryl/app)
set(AMARYL_APP_SOURCE_ROOT ${AMARYL_SOURCE_ROOT}/app)

# Embeds the application manifest (UTF-8 active code page) into a Windows executable.
function(amaryl_add_windows_manifest target)
  if(WIN32)
    target_sources(${target} PRIVATE ${AMARYL_SOURCE_ROOT}/app/amaryl.manifest)
  endif()
endfunction()

add_executable(${TARGET_APP}
  ${AMARYL_APP_INCLUDE_ROOT}/app.h
  ${AMARYL_APP_INCLUDE_ROOT}/window_metrics.h
  ${AMARYL_APP_SOURCE_ROOT}/app.cpp
  ${AMARYL_APP_SOURCE_ROOT}/main.cpp)

target_include_directories(${TARGET_APP}
  PUBLIC
  ${AMARYL_INCLUDE_ROOT})
target_link_libraries(${TARGET_APP}
  PUBLIC
  amaryl-core
  amaryl-engine
  amaryl-nrhi
  SDL3::SDL3
  plog::plog)
target_compile_definitions(${TARGET_APP}
  PUBLIC
  SDL_MAIN_USE_CALLBACKS)

if(AMARYL_WITH_EDITOR)
  target_link_libraries(${TARGET_APP} PRIVATE amaryl-editor-ui)
  target_compile_definitions(${TARGET_APP} PRIVATE AMARYL_WITH_EDITOR=1)
endif()

amaryl_add_windows_manifest(${TARGET_APP})
amaryl_inject_mimalloc(${TARGET_APP})

# Compiled Slang shaders must be present in `engine/shaders/` next to the executable.
if(TARGET amaryl-shaders)
  add_dependencies(${TARGET_APP} amaryl-shaders)
endif()
