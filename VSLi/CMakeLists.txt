cmake_minimum_required(VERSION 3.20)
project(VSL VERSION 1.0 LANGUAGES CXX)

# C++20を使用
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 静的ライブラリを作成
add_library(VSL STATIC
    ../VSLi/VSL/exceptions.cpp
    ../VSLi/VSL/Window.cpp
    ../VSLi/VSL/window_plugin.cpp
    ../VSLi/VSL/utils/ShaderCompiler.cpp
    ../VSLi/VSL/Vulkan/render_pass.cpp
    ../VSLi/VSL/Vulkan/stages/depth_stencil.cpp
    ../VSLi/VSL/Vulkan/stages/shader_group.cpp
    ../VSLi/VSL/Vulkan/stages/rasterization.cpp
    ../VSLi/VSL/Vulkan/stages/multisample.cpp
    ../VSLi/VSL/Vulkan/stages/vertex_input.cpp
    ../VSLi/VSL/Vulkan/stages/input_assembly.cpp
    ../VSLi/VSL/Vulkan/stages/color_blend.cpp
    ../VSLi/VSL/Vulkan/pipeline_layout.cpp
    ../VSLi/VSL/Vulkan/command.cpp
    ../VSLi/VSL/Vulkan/device.cpp
    ../VSLi/VSL/Vulkan/pipeline.cpp
    ../VSLi/VSL/Vulkan/view.cpp
    ../VSLi/VSL/Vulkan/shader.cpp
    ../VSLi/VSL/Vulkan/swapchain.cpp
    ../VSLi/VSL/Vulkan/commands/render_pass_end.cpp
    ../VSLi/VSL/Vulkan/commands/render_pass_begin.cpp
    ../VSLi/VSL/Vulkan/commands/bind_pipe_line.cpp
    ../VSLi/VSL/Vulkan/scissor.cpp
    ../VSLi/VSL/Vulkan/surface.cpp
    ../VSLi/VSL/Vulkan/frame_buffer.cpp
    ../VSLi/VSL/Vulkan/viewport.cpp
    ../VSLi/VSL/Vulkan/Vulkan.cpp
    ../VSLi/VSL/Vulkan/_pimpls.cpp
    ../VSLi/VSL/debug.cpp
)

# Vulkan SDK のパスを指定
find_package(Vulkan REQUIRED)
target_include_directories(VSL PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(VSL PRIVATE ${Vulkan_LIBRARIES})

# GLFW のパスを指定
find_package(glfw3 REQUIRED)
target_include_directories(VSL PRIVATE ${GLFW_INCLUDE_DIRS})
target_link_libraries(VSL PRIVATE glfw)

# Mac の場合の追加設定
if(APPLE)
    target_link_libraries(VSL PRIVATE "-framework Cocoa" "-framework OpenGL" "-framework IOKit")
endif()

# 出力ディレクトリを設定
set_target_properties(VSL PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

