cmake_minimum_required(VERSION 3.20)
project(VSL VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(VSL STATIC
        ./VSL/src/exceptions.cpp
        VSL/src/window.cpp
        ./VSL/src/window_plugin.cpp
        ./VSL/src/utils/spir_reflector.cpp
        ./VSL/src/utils/string.cpp
        ./VSL/src/utils/shader_compiler.cpp
        VSL/src/vulkan/render_pass.cpp
        VSL/src/vulkan/phase.cpp
        VSL/src/vulkan/descriptor_set_manager.cpp
        VSL/src/vulkan/descriptor.cpp
        VSL/src/vulkan/image.cpp
        VSL/src/vulkan/stages/depth_stencil.cpp
        VSL/src/vulkan/stages/shader_group.cpp
        VSL/src/vulkan/stages/rasterization.cpp
        VSL/src/vulkan/stages/multisample.cpp
        VSL/src/vulkan/stages/resource_binding.cpp
        VSL/src/vulkan/stages/vertex_input.cpp
        VSL/src/vulkan/stages/push_contents.cpp
        VSL/src/vulkan/stages/input_assembly.cpp
        VSL/src/vulkan/stages/dynamic_state.cpp
        VSL/src/vulkan/stages/color_blend.cpp
        VSL/src/vulkan/pipeline_layout.cpp
        VSL/src/vulkan/command.cpp
        VSL/src/vulkan/device.cpp
        VSL/src/vulkan/sampler.cpp
        VSL/src/vulkan/pipeline.cpp
        VSL/src/vulkan/synchronize.cpp
        VSL/src/vulkan/view.cpp
        VSL/src/vulkan/shader.cpp
        VSL/src/vulkan/buffer.cpp
        VSL/src/vulkan/swapchain.cpp
        VSL/src/vulkan/commands/copy_image_to_buffer.cpp
        VSL/src/vulkan/commands/render_pass_end.cpp
        VSL/src/vulkan/commands/render_pass_begin.cpp
        VSL/src/vulkan/commands/change_image_barrier.cpp
        VSL/src/vulkan/commands/draw_indexed.cpp
        VSL/src/vulkan/commands/bind_index_buffers.cpp
        VSL/src/vulkan/commands/copy_buffer_to_image.cpp
        VSL/src/vulkan/commands/bind_pipe_line.cpp
        VSL/src/vulkan/commands/bind_graphic_resource.cpp
        VSL/src/vulkan/commands/push_content.cpp
        VSL/src/vulkan/commands/bind_vertex_buffers.cpp
        VSL/src/vulkan/commands/draw.cpp
        VSL/src/vulkan/commands/dispatch.cpp
        VSL/src/vulkan/scissor.cpp
        VSL/src/vulkan/surface.cpp
        VSL/src/vulkan/frame_buffer.cpp
        VSL/src/vulkan/viewport.cpp
        VSL/src/vulkan/vulkan.cpp

        VSL/include/VSL/concepts.hpp
        VSL/include/VSL/type.hpp
        VSL/include/VSL/utils/typed_safe_resource.hpp
        VSL/include/VSL/utils/shader_compiler.hpp
        VSL/include/VSL/utils/spir_reflector.hpp
        VSL/include/VSL/utils/string.hpp
        VSL/include/VSL/utils/flags.hpp
        VSL/include/VSL/utils/vsl_array.hpp
        VSL/include/VSL/consteval_string.hpp
        VSL/include/VSL/window.hpp
        VSL/include/VSL/window_plugin.hpp
        VSL/include/VSL/test.hpp
        VSL/include/VSL/vulkan/scissor.hpp
        VSL/include/VSL/vulkan/surface.hpp
        VSL/include/VSL/vulkan/frame_buffer.hpp
        VSL/include/VSL/vulkan/viewport.hpp
        VSL/include/VSL/vulkan/vulkan.hpp
        VSL/include/VSL/vulkan/window_plugin/hook_window_resize.hpp
        VSL/include/VSL/vulkan/_pimpls.hpp
        VSL/include/VSL/vulkan/post_definition.hpp
        VSL/include/VSL/vulkan/stages/input_assembly.hpp
        VSL/include/VSL/vulkan/stages/dynamic_state.hpp
        VSL/include/VSL/vulkan/stages/color_blend.hpp
        VSL/include/VSL/vulkan/stages/resource_binding.hpp
        VSL/include/VSL/vulkan/stages/vertex_input.hpp
        VSL/include/VSL/vulkan/stages/push_contents.hpp
        VSL/include/VSL/vulkan/stages/multisample.hpp
        VSL/include/VSL/vulkan/stages/depth_stencil.hpp
        VSL/include/VSL/vulkan/stages/shader_group.hpp
        VSL/include/VSL/vulkan/stages/rasterization.hpp
        VSL/include/VSL/vulkan/swapchain.hpp
        VSL/include/VSL/vulkan/buffer_and_image_accessor.hpp
        VSL/include/VSL/vulkan/pipeline_layout.hpp
        VSL/include/VSL/vulkan/pv.hpp
        VSL/include/VSL/vulkan/command.hpp
        VSL/include/VSL/vulkan/device.hpp
        VSL/include/VSL/vulkan/sampler.hpp
        VSL/include/VSL/vulkan/pipeline.hpp
        VSL/include/VSL/vulkan/view.hpp
        VSL/include/VSL/vulkan/synchronize.hpp
        VSL/include/VSL/vulkan/buffer.hpp
        VSL/include/VSL/vulkan/shader.hpp
        VSL/include/VSL/vulkan/render_pass.hpp
        VSL/include/VSL/vulkan/commands/dispatch.hpp
        VSL/include/VSL/vulkan/commands/push_content.hpp
        VSL/include/VSL/vulkan/commands/bind_vertex_buffers.hpp
        VSL/include/VSL/vulkan/commands/draw.hpp
        VSL/include/VSL/vulkan/commands/copy_buffer_to_image.hpp
        VSL/include/VSL/vulkan/commands/bind_pipe_line.hpp
        VSL/include/VSL/vulkan/commands/bind_graphic_resource.hpp
        VSL/include/VSL/vulkan/commands/copy_image_to_buffer.hpp
        VSL/include/VSL/vulkan/commands/render_pass_end.hpp
        VSL/include/VSL/vulkan/commands/render_pass_begin.hpp
        VSL/include/VSL/vulkan/commands/change_image_barrier.hpp
        VSL/include/VSL/vulkan/commands/draw_indexed.hpp
        VSL/include/VSL/vulkan/commands/bind_index_buffers.hpp
        VSL/include/VSL/vulkan/phase.hpp
        VSL/include/VSL/vulkan/descriptor_set_manager.hpp
        VSL/include/VSL/vulkan/descriptor.hpp
        VSL/include/VSL/vulkan/_dimension.hpp
        VSL/include/VSL/vulkan/image.hpp
        VSL/include/VSL/dimension.hpp
        VSL/include/VSL/exceptions.hpp
        VSL/include/VSL/format.hpp
        VSL/include/VSL/vsl.hpp
        VSL/include/VSL/define.hpp
)

target_include_directories(VSL PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(VSL PRIVATE ${Vulkan_LIBRARIES})
target_link_libraries(VSL PRIVATE
        "${Vulkan_LIBRARY_DIRS}/libspirv-cross-core.a"
        "${Vulkan_LIBRARY_DIRS}/libspirv-cross-cpp.a"
        "${Vulkan_LIBRARY_DIRS}/libspirv-cross-glsl.a"
        "${Vulkan_LIBRARY_DIRS}/libspirv-cross-hlsl.a"
        "${Vulkan_LIBRARY_DIRS}/libspirv-cross-msl.a"
        "${Vulkan_LIBRARY_DIRS}/libspirv-cross-reflect.a"
        "${Vulkan_LIBRARY_DIRS}/libspirv-cross-util.a"
)


target_include_directories(VSL PRIVATE ${GLFW_INCLUDE_DIRS} ${VSL_HOME}/include/)
target_link_libraries(VSL PRIVATE glfw)

if(APPLE)
    target_link_libraries(VSL PRIVATE "-framework Cocoa" "-framework OpenGL" "-framework IOKit")
endif()

set_target_properties(VSL PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

install(TARGETS VSL
        EXPORT VSLTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/VSLi/VSL/include/
        DESTINATION include
)

install(EXPORT VSLTargets
        FILE VSLTargets.cmake
        NAMESPACE VSL::
        DESTINATION share/VSL/cmake
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
        ${CMAKE_BINARY_DIR}/share/VSL/cmake/VSLConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
        ${CMAKE_SOURCE_DIR}/VSLi/VSLConfig.cmake.in
        ${CMAKE_BINARY_DIR}/share/VSL/cmake/VSLConfig.cmake
        INSTALL_DESTINATION share/VSL/cmake
)

install(
        FILES
        ${CMAKE_BINARY_DIR}/share/VSL/cmake/VSLConfig.cmake
        ${CMAKE_BINARY_DIR}/share/VSL/cmake/VSLConfigVersion.cmake
        DESTINATION share/VSL/cmake
)