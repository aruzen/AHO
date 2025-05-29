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
        ../AHO/Main.cpp)

# ライブラリをリンク
link_directories(lib)

find_package(Boost REQUIRED COMPONENTS system)
target_include_directories(AHO PRIVATE ${Boost_INCLUDE_DIRS} ~/repos/AHO/AHOLi ~/repos/AHO/VSLi)
target_link_libraries(AHO PRIVATE ${Boost_LIBRARIES} VSL)
