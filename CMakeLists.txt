# CMakeList.txt : CMake project for vulkan_guide, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 3.20)

project(AHO-all)
set(CMAKE_C_COMPILER "/opt/homebrew/Cellar/gcc/14.2.0_1/bin/gcc-14" CACHE STRING "clang compiler" FORCE)
set(CMAKE_CXX_COMPILER "/opt/homebrew/Cellar/gcc/14.2.0_1/bin/g++-14" CACHE STRING "clang++ compiler" FORCE)
string(REPLACE "-fcolor-diagnostics" "-fdiagnostics-color=auto" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# VulkanとGLFWの設定
find_package(Boost)
find_package(Vulkan REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)

add_subdirectory(AHO)
add_subdirectory(AHOLi)
add_subdirectory(Dev)
add_subdirectory(VSLi)

# GLFWのインクルードディレクトリとリンクディレクトリを設定
include_directories(${GLFW_INCLUDE_DIRS})
link_directories(${GLFW_LIBRARY_DIRS})

# MacOS特有のフラグを追加 (MetalとMoltenVKのサポート)
if(APPLE)
    add_definitions(-DVK_USE_PLATFORM_METAL_EXT)
endif()

# 実行ファイルを生成
# add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# VulkanとGLFWをリンク
# target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan ${GLFW_LIBRARIES})

