# stb_image v2.34, https://github.com/nothings/stb (stb_image.h only)
add_library(stb SHARED
  ${AMARYL_3RDPARTY_ROOT}/stb/stb_image.h
  ${AMARYL_3RDPARTY_ROOT}/stb/stb_image.cpp
)

target_include_directories(stb PUBLIC ${AMARYL_3RDPARTY_ROOT}/stb)
if(WIN32)
  target_compile_definitions(stb
    PRIVATE "STBIDEF=extern __declspec(dllexport)"
    INTERFACE "STBIDEF=extern __declspec(dllimport)"
  )
endif()
set_target_properties(stb
  PROPERTIES
  FOLDER "3rdparty"
)
