cmake_minimum_required(VERSION 3.20)
project(AHO VERSION 1.0 LANGUAGES CXX)

# C++20を使用
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# リテラルの_がないことを無視する
add_compile_options(-Wno-user-defined-literals)

# インクルードディレクトリを追加
include_directories(~/repos/AHO/AHOLi ~/repos/AHO/VSLi)

# ライブラリディレクトリを追加
link_directories(lib)

# 実行ファイルを作成
add_executable(AHO
        ../AHO/Main.cpp)


# ライブラリをリンク
target_link_libraries(AHO VSL)