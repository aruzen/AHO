project(vsl-test LANGUAGES CXX)

add_executable(vsl-test
        smoke.cpp
        dimension_test.cpp
        format_test.cpp
        order_regression_test.cpp
        string_exception_test.cpp
)

target_include_directories(vsl-test
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../include"
)

target_link_libraries(vsl-test
        PRIVATE
        VSL
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(vsl-test)
