cmake_minimum_required(VERSION 3.20)
project(AHO VERSION 1.0 LANGUAGES CXX)

# C++20を使用
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# リテラルの_がないことを無視する
add_compile_options(-Wno-user-defined-literals)
add_compile_options(-fconcepts-diagnostics-depth=7)

# 実行ファイルを作成
add_executable(AHO
        ../AHO/Main.cpp
        ../AHOLi/AHO/io/Input.cpp
        ../AHOLi/AHO/io/Input.hpp
        ../AHOLi/AHO/io/InputManager.cpp
        ../AHOLi/AHO/io/InputManager.hpp
        ../AHOLi/AHO/io/Key.cpp
        ../AHOLi/AHO/io/Key.hpp
        ../AHOLi/AHO/io/KeyBoard.cpp
        ../AHOLi/AHO/io/KeyBoard.hpp
        ../AHOLi/AHO/io/Mouse.cpp
        ../AHOLi/AHO/Engine.cpp
)

# ライブラリをリンク
link_directories(lib)

# TODO: ~/repos/AHO/AHOLi ~/repos/AHO/VSLi & VSL これダメ
target_include_directories(AHO PRIVATE ${Boost_INCLUDE_DIRS} ~/repos/AHO/AHOLi ~/repos/AHO/VSLi)
target_link_libraries(AHO PRIVATE ${Boost_LIBRARIES} VSL)

target_include_directories(AHO PRIVATE ${GLFW_INCLUDE_DIRS})
target_link_directories(AHO PRIVATE ${GLFW_LIBRARY_DIRS})
target_link_libraries(AHO PRIVATE ${GLFW_LIBRARIES})

find_package(glm CONFIG REQUIRED)
target_link_libraries(AHO PRIVATE glm::glm)