# Add source to this project's executable.
# add_executable(AHOLi)
project(vsl-aho-test)

find_package(GTest REQUIRED)
message("GoogleTest: ${GTest_VERSION}")

enable_testing()

add_executable(vsl-aho-test
        ./VSL/SPIRVReflector.cpp
)

target_link_libraries(vsl-aho-test
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(vsl-aho-test)