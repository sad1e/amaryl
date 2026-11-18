add_library(cgltf STATIC
	${AMARYL_3RDPARTY_ROOT}/cgltf/cgltf.h
	${AMARYL_3RDPARTY_ROOT}/cgltf/cgltf_write.h
	${AMARYL_3RDPARTY_ROOT}/cgltf/cgltf.cpp
)

target_include_directories(cgltf PUBLIC ${AMARYL_3RDPARTY_ROOT})
set_target_properties(cgltf
	PROPERTIES FOLDER "3rdparty"
)
