cmake_minimum_required(VERSION 3.20)
project(AHO VERSION 1.0 LANGUAGES CXX)

# 実行ファイルを作成
add_executable(AHO
        Main.cpp
)

# ライブラリをリンク
link_directories(lib)

target_include_directories(AHO PRIVATE ${Boost_INCLUDE_DIRS} ${AHO_HOME}/include ${VSL_HOME}/include)
target_link_libraries(AHO PRIVATE ${Boost_LIBRARIES} VSL AHOLi)

target_include_directories(AHO PRIVATE ${GLFW_INCLUDE_DIRS})
target_link_directories(AHO PRIVATE ${GLFW_LIBRARY_DIRS})
target_link_libraries(AHO PRIVATE ${GLFW_LIBRARIES})

find_package(glm CONFIG REQUIRED)
target_link_libraries(AHO PRIVATE glm::glm)

target_include_directories(AHO PRIVATE ../thirdparty/)