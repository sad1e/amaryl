add_library(pystring STATIC
  ${AMARYL_3RDPARTY_ROOT}/pystring/pystring.h
  ${AMARYL_3RDPARTY_ROOT}/pystring/pystring.cpp
)

target_include_directories(pystring PUBLIC ${AMARYL_3RDPARTY_ROOT})
set_target_properties(pystring
  PROPERTIES
  FOLDER "3rdparty"
)
