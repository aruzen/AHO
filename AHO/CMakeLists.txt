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

if(NOT TARGET glm::glm)
find_package(glm CONFIG REQUIRED)
endif()
target_link_libraries(AHO PRIVATE glm::glm)

target_include_directories(AHO PRIVATE ../thirdparty/)

aho_copy_glfw_runtime(AHO)

if(WIN32)
    set_target_properties(AHO PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            VS_DEBUGGER_ENVIRONMENT "AHO_HOME=${AHO_HOME}\nPATH=${VULKAN_SDK}/Bin;%PATH%"
    )
endif()
