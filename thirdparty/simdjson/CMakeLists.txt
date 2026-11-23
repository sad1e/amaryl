# simdjson v5.3.0 single header amalgamation, https://github.com/simdjson/simdjson
# (singleheader/simdjson.h and singleheader/simdjson.cpp)
add_library(simdjson SHARED
  ${AMARYL_3RDPARTY_ROOT}/simdjson/simdjson.h
  ${AMARYL_3RDPARTY_ROOT}/simdjson/simdjson.cpp
)

target_include_directories(simdjson PUBLIC ${AMARYL_3RDPARTY_ROOT})
target_compile_definitions(simdjson PUBLIC SIMDJSON_DISABLE_DEPRECATED_API)
if(WIN32)
  target_compile_definitions(simdjson
    PRIVATE SIMDJSON_BUILDING_WINDOWS_DYNAMIC_LIBRARY=1
    INTERFACE SIMDJSON_USING_WINDOWS_DYNAMIC_LIBRARY=1
  )
endif()
set_target_properties(simdjson
  PROPERTIES
  FOLDER "3rdparty"
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)
