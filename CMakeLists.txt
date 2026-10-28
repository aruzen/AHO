# CMakeList.txt : CMake project for vulkan_guide, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0167 NEW)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(NOT DEFINED AHO_HOME)
    set(AHO_HOME $ENV{AHO_HOME})
endif ()
if (NOT DEFINED VSL_HOME)
    set(VSL_HOME $ENV{VSL_HOME})
endif ()
message(STATUS "AHO_HOME : ${AHO_HOME}")
message(STATUS "VSL_HOME : ${VSL_HOME}")

project(AHO-all)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(AHO_STYLE_CHECKS "Enable AHO/VSL format, naming and layout checks" OFF)
if(AHO_STYLE_CHECKS)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

include(CTest)
if (BUILD_TESTING)
    find_package(GTest REQUIRED)
    include(GoogleTest)
endif ()

include(cmake/BuildOptions.cmake)

# setting Libraries
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
find_package(glfw3 CONFIG REQUIRED)
message("glfw3: ${GLFW_INCLUDE_DIRS}, ${GLFW_LIBRARY_DIRS}, ${GLFW_LIBRARIES}")

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
