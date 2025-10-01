# CMakeList.txt : CMake project for vulkan_guide, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 3.10...3.28)
cmake_policy(SET CMP0167 NEW)

project(AHO-all)
set(CMAKE_C_COMPILER "/opt/homebrew/Cellar/gcc/15.1.0/bin/gcc-15" CACHE STRING "clang compiler" FORCE)
set(CMAKE_CXX_COMPILER "/opt/homebrew/Cellar/gcc/15.1.0/bin/g++-15" CACHE STRING "clang++ compiler" FORCE)
string(REPLACE "-fcolor-diagnostics" "-fdiagnostics-color=auto" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# setting Libraries
find_package(PkgConfig REQUIRED)
find_package(Boost COMPONENTS system QUIET)
if(NOT Boost_FOUND)
    message(STATUS "Boost Config mode failed. Retrying with FindBoost...")
    # add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY)
    set(Boost_NO_BOOST_CMAKE ON)
    find_package(Boost REQUIRED COMPONENTS system)
endif()
message("Boost: ${Boost_INCLUDE_DIRS}, ${Boost_LIBRARY_DIRS}, ${Boost_LIBRARIES}")

find_package(Vulkan REQUIRED)
message("Vulkan: ${Vulkan_INCLUDE_DIRS}, ${Vulkan_LIBRARIES}")
pkg_check_modules(GLFW REQUIRED glfw3)
message("glfw3: ${GLFW_INCLUDE_DIRS}, ${GLFW_LIBRARY_DIRS}, ${GLFW_LIBRARIES}")

add_subdirectory(AHO)
add_subdirectory(AHOLi)
add_subdirectory(Dev)
add_subdirectory(VSLi)
add_subdirectory(Fractal)

include_directories(${GLFW_INCLUDE_DIRS})
link_directories(${GLFW_LIBRARY_DIRS})

if(APPLE)
    add_definitions(-DVK_USE_PLATFORM_METAL_EXT)
endif()