include(CTest)

if(BUILD_TESTING)
    add_executable(amaryl-math-test
        ${AMARYL_ROOT}/tests/core/math_test.cpp
        ${AMARYL_CORE_SOURCE_ROOT}/math/mat44.cpp
        ${AMARYL_CORE_SOURCE_ROOT}/math/quat.cpp
        ${AMARYL_CORE_SOURCE_ROOT}/math/rotator.cpp
        ${AMARYL_CORE_SOURCE_ROOT}/math/vec3.cpp
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
endif()
