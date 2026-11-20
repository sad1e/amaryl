include(CTest)

if(BUILD_TESTING)
    add_executable(amaryl-math-test
        ${AMARYL_ROOT}/tests/core/math_test.cpp
    )

    target_link_libraries(amaryl-math-test PRIVATE amaryl-core)
    target_include_directories(amaryl-math-test PRIVATE ${AMARYL_ROOT}/tests)

    add_test(NAME amaryl-math COMMAND amaryl-math-test)

    add_executable(amaryl-allocator-test
        ${AMARYL_ROOT}/tests/core/allocator_test.cpp
    )

    target_link_libraries(amaryl-allocator-test PRIVATE amaryl-core)
    target_include_directories(amaryl-allocator-test PRIVATE ${AMARYL_ROOT}/tests)

    add_test(NAME amaryl-allocator COMMAND amaryl-allocator-test)

    add_executable(amaryl-uuid-test
        ${AMARYL_ROOT}/tests/core/uuid_test.cpp
    )

    target_link_libraries(amaryl-uuid-test PRIVATE amaryl-core)
    target_include_directories(amaryl-uuid-test PRIVATE ${AMARYL_ROOT}/tests)

    add_test(NAME amaryl-uuid COMMAND amaryl-uuid-test)

    add_executable(amaryl-scene-graph-test
        ${AMARYL_ROOT}/tests/engine/scene_graph_test.cpp
    )

    target_link_libraries(amaryl-scene-graph-test PRIVATE amaryl-engine)
    target_include_directories(amaryl-scene-graph-test PRIVATE ${AMARYL_ROOT}/tests)

    add_test(NAME amaryl-scene-graph COMMAND amaryl-scene-graph-test)

    add_executable(amaryl-mesh-asset-test
        ${AMARYL_ROOT}/tests/engine/mesh_asset_test.cpp
    )

    target_link_libraries(amaryl-mesh-asset-test PRIVATE amaryl-engine)
    target_include_directories(amaryl-mesh-asset-test PRIVATE ${AMARYL_ROOT}/tests)

    add_test(NAME amaryl-mesh-asset COMMAND amaryl-mesh-asset-test)

    add_executable(amaryl-gpu-scene-test
        ${AMARYL_ROOT}/tests/engine/gpu_scene_test.cpp
    )

    target_link_libraries(amaryl-gpu-scene-test PRIVATE amaryl-engine)
    target_include_directories(amaryl-gpu-scene-test PRIVATE ${AMARYL_ROOT}/tests)

    add_test(NAME amaryl-gpu-scene COMMAND amaryl-gpu-scene-test)

    add_executable(amaryl-gltf-import-test
        ${AMARYL_ROOT}/tests/engine/gltf_import_test.cpp
    )

    target_link_libraries(amaryl-gltf-import-test PRIVATE amaryl-engine)
    target_include_directories(amaryl-gltf-import-test PRIVATE ${AMARYL_ROOT}/tests)

    add_test(NAME amaryl-gltf-import COMMAND amaryl-gltf-import-test)

    add_executable(amaryl-entity-test
        ${AMARYL_ROOT}/tests/engine/entity_test.cpp
    )

    target_link_libraries(amaryl-entity-test PRIVATE amaryl-engine)
    target_include_directories(amaryl-entity-test PRIVATE ${AMARYL_ROOT}/tests)

    add_test(NAME amaryl-entity COMMAND amaryl-entity-test)

    add_executable(amaryl-camera-test
        ${AMARYL_ROOT}/tests/engine/camera_test.cpp
    )

    target_link_libraries(amaryl-camera-test PRIVATE amaryl-engine)
    target_include_directories(amaryl-camera-test PRIVATE ${AMARYL_ROOT}/tests)

    add_test(NAME amaryl-camera COMMAND amaryl-camera-test)
endif()
