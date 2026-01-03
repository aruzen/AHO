# CMakeList.txt : CMake project for vulkan_guide, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 3.10...3.28)
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
string(REPLACE "-fcolor-diagnostics" "-fdiagnostics-color=auto" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Debug option
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -DDEBUG -fstandalone-debug -fno-omit-frame-pointer")
    set(CMAKE_C_FLAGS_DEBUG   "-O0 -g3 -DDEBUG -fstandalone-debug -fno-omit-frame-pointer")
    add_compile_options(-Wno-user-defined-literals)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-fconcepts-diagnostics-depth=7)
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g2 -DDEBUG -gdwarf-4 -fvar-tracking-assignments -fno-omit-frame-pointer")
    set(CMAKE_C_FLAGS_DEBUG   "-O0 -g2 -DDEBUG -gdwarf-4 -fvar-tracking-assignments -fno-omit-frame-pointer")
endif()

# Release option
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -ftree-vectorize")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -march=native -ftree-vectorize")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -ftree-vectorize -fomit-frame-pointer")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -march=native -ftree-vectorize -fomit-frame-pointer")
endif()

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
message("glfw3: ${GLFW_INCLUDE_DIRS}, ${GLFW_LIBRARY_DIRS}, ${GLFW_LIBRARIES}")

add_subdirectory(AHO)
add_subdirectory(AHOLi)
add_subdirectory(VSLi)
# add_subdirectory()

include_directories(${GLFW_INCLUDE_DIRS})
link_directories(${GLFW_LIBRARY_DIRS})

if (APPLE)
    add_definitions(-DVK_USE_PLATFORM_METAL_EXT)
endif ()