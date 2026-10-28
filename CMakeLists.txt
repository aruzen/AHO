# CMakeList.txt : CMake project for vulkan_guide, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0167 NEW)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(NOT DEFINED AHO_HOME)
    if(DEFINED ENV{AHO_HOME} AND NOT "$ENV{AHO_HOME}" STREQUAL "")
        set(AHO_HOME "$ENV{AHO_HOME}" CACHE PATH "AHO library root")
    else()
        set(AHO_HOME "${CMAKE_CURRENT_SOURCE_DIR}/AHOLi/AHO" CACHE PATH "AHO library root")
    endif()
endif ()
if (NOT DEFINED VSL_HOME)
    if(DEFINED ENV{VSL_HOME} AND NOT "$ENV{VSL_HOME}" STREQUAL "")
        set(VSL_HOME "$ENV{VSL_HOME}" CACHE PATH "VSL library root")
    else()
        set(VSL_HOME "${CMAKE_CURRENT_SOURCE_DIR}/VSLi/VSL" CACHE PATH "VSL library root")
    endif()
endif ()
message(STATUS "AHO_HOME : ${AHO_HOME}")
message(STATUS "VSL_HOME : ${VSL_HOME}")

project(AHO-all)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

if(MSVC)
    # CMake maps MSVC's latest mode to C++23, although this project uses
    # the same mode for its C++26 build and checks optional features separately.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND NOT "cxx_std_26" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
        set(CMAKE_CXX26_STANDARD_COMPILE_OPTION "/std:c++latest")
        set(CMAKE_CXX26_EXTENSION_COMPILE_OPTION "/std:c++latest")
        list(APPEND CMAKE_CXX_COMPILE_FEATURES cxx_std_26)
    endif()
endif()

include(cmake/WindowsDependencies.cmake)

option(AHO_STYLE_CHECKS "Enable AHO/VSL format, naming and layout checks" OFF)
if(AHO_STYLE_CHECKS)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

include(CTest)
if (BUILD_TESTING)
    set(AHO_GOOGLETEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/googletest" CACHE PATH "Local GoogleTest source directory")
    if(EXISTS "${AHO_GOOGLETEST_ROOT}/CMakeLists.txt")
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        add_subdirectory("${AHO_GOOGLETEST_ROOT}" "${CMAKE_BINARY_DIR}/thirdparty/googletest")
    else()
        find_package(GTest REQUIRED)
    endif()
    include(GoogleTest)
endif ()

include(cmake/BuildOptions.cmake)

# setting Libraries
if(NOT WIN32)
find_package(PkgConfig REQUIRED)
find_package(Boost COMPONENTS system QUIET)
if (NOT Boost_FOUND)
    message(STATUS "Boost Config mode failed. Retrying with FindBoost...")
    # add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY)
    set(Boost_NO_BOOST_CMAKE ON)
    find_package(Boost REQUIRED COMPONENTS system)
endif ()
message("Boost: ${Boost_INCLUDE_DIRS}, ${Boost_LIBRARY_DIRS}, ${Boost_LIBRARIES}")

find_package(Vulkan REQUIRED)
get_filename_component(Vulkan_LIBRARY_DIRS "${Vulkan_LIBRARIES}" DIRECTORY)
message("Vulkan: ${Vulkan_INCLUDE_DIRS}, ${Vulkan_LIBRARIES}")
pkg_check_modules(GLFW REQUIRED glfw3)
message("glfw3: ${GLFW_INCLUDE_DIRS}, ${GLFW_LIBRARY_DIRS}, ${GLFW_LIBRARIES}")
endif()

add_subdirectory(AHO)
add_subdirectory(AHOLi)
add_subdirectory(VSLi)

if(AHO_STYLE_CHECKS)
    add_subdirectory(tools/style)
endif()

if (BUILD_TESTING)
    add_custom_target(build-tests DEPENDS aho-tests vsl-tests)
    add_custom_target(check
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C "$<CONFIG>"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS build-tests
            USES_TERMINAL
            VERBATIM
    )
endif ()
# add_subdirectory()

include_directories(${GLFW_INCLUDE_DIRS})
link_directories(${GLFW_LIBRARY_DIRS})

if (APPLE)
    add_definitions(-DVK_USE_PLATFORM_METAL_EXT)
endif ()
