# meshoptimizer v1.3, https://github.com/zeux/meshoptimizer
set(MESHOPTIMIZER_ROOT ${AMARYL_3RDPARTY_ROOT}/meshoptimizer/src)

add_library(meshoptimizer STATIC
	${MESHOPTIMIZER_ROOT}/meshoptimizer.h
	${MESHOPTIMIZER_ROOT}/allocator.cpp
	${MESHOPTIMIZER_ROOT}/clusterizer.cpp
	${MESHOPTIMIZER_ROOT}/indexanalyzer.cpp
	${MESHOPTIMIZER_ROOT}/indexcodec.cpp
	${MESHOPTIMIZER_ROOT}/indexgenerator.cpp
	${MESHOPTIMIZER_ROOT}/meshletcodec.cpp
	${MESHOPTIMIZER_ROOT}/meshletutils.cpp
	${MESHOPTIMIZER_ROOT}/opacitymap.cpp
	${MESHOPTIMIZER_ROOT}/overdrawoptimizer.cpp
	${MESHOPTIMIZER_ROOT}/partition.cpp
	${MESHOPTIMIZER_ROOT}/quantization.cpp
	${MESHOPTIMIZER_ROOT}/rasterizer.cpp
	${MESHOPTIMIZER_ROOT}/remesher.cpp
	${MESHOPTIMIZER_ROOT}/simplifier.cpp
	${MESHOPTIMIZER_ROOT}/spatialorder.cpp
	${MESHOPTIMIZER_ROOT}/stripifier.cpp
	${MESHOPTIMIZER_ROOT}/tangentspace.cpp
	${MESHOPTIMIZER_ROOT}/vcacheoptimizer.cpp
	${MESHOPTIMIZER_ROOT}/vertexcodec.cpp
	${MESHOPTIMIZER_ROOT}/vertexfilter.cpp
	${MESHOPTIMIZER_ROOT}/vfetchoptimizer.cpp
)

target_include_directories(meshoptimizer PUBLIC ${MESHOPTIMIZER_ROOT})
set_target_properties(meshoptimizer
	PROPERTIES FOLDER "3rdparty"
	POSITION_INDEPENDENT_CODE ON
)
