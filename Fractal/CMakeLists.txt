cmake_minimum_required(VERSION 3.20)
project(Fractal VERSION 1.0 LANGUAGES CXX)

# C++20を使用
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# リテラルの_がないことを無視する
add_compile_options(-Wno-user-defined-literals)
add_compile_options(-fconcepts-diagnostics-depth=7)

# 実行ファイルを作成
add_executable(Fractal
        ../Fractal/Main.cpp
        ../AHOLi/AHO/io/Input.cpp
        ../AHOLi/AHO/io/Input.hpp
        ../AHOLi/AHO/io/InputManager.cpp
        ../AHOLi/AHO/io/InputManager.hpp
        ../AHOLi/AHO/io/Key.cpp
        ../AHOLi/AHO/io/Key.hpp
        ../AHOLi/AHO/io/KeyBoard.cpp
        ../AHOLi/AHO/io/KeyBoard.hpp
)

# ライブラリをリンク
link_directories(lib)

find_package(Boost REQUIRED COMPONENTS system)
message("boost: ${Boost_INCLUDE_DIRS}, ${Boost}")
target_include_directories(Fractal PRIVATE ${Boost_INCLUDE_DIRS} ~/repos/AHO/AHOLi ~/repos/AHO/VSLi)
target_link_libraries(Fractal PRIVATE ${Boost_LIBRARIES} VSL)

find_package(glfw3 REQUIRED)
target_include_directories(Fractal PRIVATE ${GLFW_INCLUDE_DIRS})
target_link_libraries(Fractal PRIVATE glfw)