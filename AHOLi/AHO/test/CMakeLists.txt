project(aho-test LANGUAGES CXX)

add_executable(aho-test
        smoke.cpp
        core_angle_test.cpp
        core_color_test.cpp
        core_index_test.cpp
        core_line_triangle_test.cpp
        core_matrix_test.cpp
        core_point_vector_test.cpp
        io_input_test.cpp
        resource_buffer_test.cpp
)

target_include_directories(aho-test
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../include"
        "${VSL_HOME}/include"
)

target_link_libraries(aho-test
        PRIVATE
        AHOLi
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(aho-test)
