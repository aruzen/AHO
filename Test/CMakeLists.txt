# Add source to this project's executable.
# add_executable(AHOLi)
cmake_minimum_required(VERSION 3.30)
project(vsl-aho-test)

find_package(GTest REQUIRED)
message("GoogleTest: ${GTest_VERSION}")

enable_testing()

add_executable(vsl-aho-test
        ./VSL/SPIRVReflector.cpp
)

target_compile_features(vsl-aho-test PRIVATE cxx_std_26)

target_link_libraries(vsl-aho-test
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(vsl-aho-test)
