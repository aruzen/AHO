cmake_minimum_required(VERSION 3.30)
project(AHOInstalledConsumer LANGUAGES CXX)

find_package(AHO CONFIG REQUIRED)
add_executable(aho-consumer main.cpp)
target_link_libraries(aho-consumer PRIVATE AHO::AHOLi)

include(CTest)
add_test(NAME AHO.InstalledConsumer COMMAND aho-consumer)
