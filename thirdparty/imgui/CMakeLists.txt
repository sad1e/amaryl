# Dear ImGui v1.93.0-docking, https://github.com/ocornut/imgui
# (core files only, backends are not included)
add_library(imgui SHARED
  ${AMARYL_3RDPARTY_ROOT}/imgui/imconfig.h
  ${AMARYL_3RDPARTY_ROOT}/imgui/imgui.h
  ${AMARYL_3RDPARTY_ROOT}/imgui/imgui_internal.h
  ${AMARYL_3RDPARTY_ROOT}/imgui/imstb_rectpack.h
  ${AMARYL_3RDPARTY_ROOT}/imgui/imstb_textedit.h
  ${AMARYL_3RDPARTY_ROOT}/imgui/imstb_truetype.h
  ${AMARYL_3RDPARTY_ROOT}/imgui/imgui.cpp
  ${AMARYL_3RDPARTY_ROOT}/imgui/imgui_demo.cpp
  ${AMARYL_3RDPARTY_ROOT}/imgui/imgui_draw.cpp
  ${AMARYL_3RDPARTY_ROOT}/imgui/imgui_tables.cpp
  ${AMARYL_3RDPARTY_ROOT}/imgui/imgui_widgets.cpp
  ${AMARYL_3RDPARTY_ROOT}/imgui/imgui.natvis
)

target_include_directories(imgui PUBLIC ${AMARYL_3RDPARTY_ROOT}/imgui)
if(WIN32)
  target_compile_definitions(imgui
    PRIVATE "IMGUI_API=__declspec(dllexport)"
    INTERFACE "IMGUI_API=__declspec(dllimport)"
  )
endif()
set_target_properties(imgui
  PROPERTIES
  FOLDER "3rdparty"
)
