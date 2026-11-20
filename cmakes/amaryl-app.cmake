project(amaryl-app)
set(TARGET_EDITOR amaryl-editor)

set(AMARYL_APP_INCLUDE_ROOT ${AMARYL_INCLUDE_ROOT}/amaryl/app)
set(AMARYL_APP_SOURCE_ROOT ${AMARYL_SOURCE_ROOT}/app)

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
