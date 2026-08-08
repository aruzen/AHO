project(vsl-test LANGUAGES CXX)

add_executable(vsl-test
        smoke.cpp
)

target_link_libraries(vsl-test
        PRIVATE
        VSL
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(vsl-test)
