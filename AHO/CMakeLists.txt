cmake_minimum_required(VERSION 3.30)
project(AHO VERSION 1.0 LANGUAGES CXX)

# 実行ファイルを作成
add_executable(AHO
        Main.cpp
)
aho_target_build_options(AHO)

target_link_libraries(AHO PRIVATE AHO::AHOLi)
target_include_directories(AHO SYSTEM PRIVATE "${AHO_STB_INCLUDE_DIR}")
target_compile_definitions(AHO PRIVATE
        AHO_SAMPLE_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        AHO_SAMPLE_RESOURCE_DIR="${AHO_HOME}/built-in-resource")

if(WIN32)
    set_target_properties(AHO PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            VS_DEBUGGER_ENVIRONMENT "AHO_HOME=${AHO_HOME}\nPATH=${VULKAN_SDK}/Bin;%PATH%")
endif()
aho_copy_windows_runtime()
