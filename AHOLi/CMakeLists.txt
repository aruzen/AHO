cmake_minimum_required(VERSION 3.30)
project(AHOLi VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/Saitekikuukan.cmake)
include(cmake/TinyGltf.cmake)
option(AHO_ENABLE_MSDF "Build CPU MSDF font, atlas and text layout resources" OFF)

add_library(AHOLi
        AHO/src/pipeline/layouts.cpp
        AHO/src/pipeline/pipeline.cpp
        AHO/src/window.cpp
        AHO/src/render_context.cpp
        AHO/include/AHO/render_context.hpp
        AHO/src/io/input.cpp
        AHO/src/io/mouse.cpp
        AHO/src/io/input_manager.cpp
        AHO/src/io/keyboard.cpp
        AHO/src/io/key.cpp
        AHO/src/drawer.cpp
        AHO/src/image_readback.cpp
        AHO/src/x/resource/gltf.cpp
        AHO/src/x/resource/shader.cpp
        AHO/src/x/screenshot.cpp
        AHO/src/x/resource/texture.cpp
        AHO/include/AHO/x/resource/texture.hpp
        AHO/include/AHO/x/resource/typed_texture.hpp
        AHO/include/AHO/resource/typed_buffer.hpp
        AHO/src/engine.cpp
        AHO/src/resource/image.cpp
        AHO/src/resource/buffer.cpp
        AHO/src/command/buffer_upload.cpp
        AHO/include/AHO/command/buffer_upload.hpp
        AHO/src/x/command/texture_update.cpp
        AHO/include/AHO/x/command/texture_update.hpp
        AHO/src/x/resource/texture_impl.hpp
        AHO/src/engine/standard_engine.cpp
        AHO/src/engine/graphical_engine.cpp

        AHO/include/AHO/image_readback.hpp
        AHO/include/AHO/x/resource/gltf.hpp
        AHO/include/AHO/x/resource/shader.hpp
        AHO/include/AHO/x/screenshot.hpp
        AHO/include/AHO/engine.hpp
        AHO/include/AHO/concepts.hpp
        AHO/include/AHO/pipeline/layouts.hpp
        AHO/include/AHO/pipeline/pipeline.hpp
        AHO/include/AHO/drawer/standard_drawer.hpp
        AHO/include/AHO/core/collision.hpp
        AHO/include/AHO/core/index.hpp
        AHO/include/AHO/core/vector.hpp
        AHO/include/AHO/core/triangle.hpp
        AHO/include/AHO/core/point.hpp
        AHO/include/AHO/core/triangulation.hpp
        AHO/include/AHO/core/detail/geometry2d.hpp
        AHO/include/AHO/core/detail/soa_storage.hpp
        AHO/include/AHO/core/polygon.hpp
        AHO/include/AHO/core/math/angle.hpp
        AHO/include/AHO/core/math/mat.hpp
        AHO/include/AHO/core/math/functions.hpp
        AHO/include/AHO/core/math/transform.hpp
        AHO/include/AHO/core/math/conventions.hpp
        AHO/include/AHO/core/math/matrix_functions.hpp
        AHO/include/AHO/core/math/projection.hpp
        AHO/include/AHO/core/math/quaternion.hpp
        AHO/include/AHO/core/math/coordinate.hpp
        AHO/include/AHO/core/color.hpp
        AHO/include/AHO/core/line.hpp
        AHO/include/AHO/core/ov.hpp
        AHO/include/AHO/io/keyboard.hpp
        AHO/include/AHO/io/key.hpp
        AHO/include/AHO/io/input_manager.hpp
        AHO/include/AHO/io/mouse.hpp
        AHO/include/AHO/io/input.hpp
        AHO/include/AHO/drawer.hpp
        AHO/include/AHO/utils/polygon.hpp
        AHO/include/AHO/window.hpp
        AHO/include/AHO/object/object.hpp
        AHO/include/AHO/resource/audio.hpp
        AHO/include/AHO/resource/buffer.hpp
        AHO/include/AHO/resource/object_hitbox.hpp
        AHO/include/AHO/resource/image.hpp
        AHO/include/AHO/resource/object_model.hpp
        AHO/include/AHO/engine/graphical_engine.hpp
        AHO/include/AHO/engine/standard_engine.hpp
        AHO/include/AHO/define.hpp
        AHO/src/drawer/draw_phase.cpp
        AHO/src/drawer/standard_drawer.cpp
)

target_compile_features(AHOLi PUBLIC cxx_std_26)
aho_target_build_options(AHOLi)
target_include_directories(AHOLi SYSTEM PRIVATE "${AHO_TINYGLTF_INCLUDE_DIR}")

target_include_directories(AHOLi
        PRIVATE
        ${Boost_INCLUDE_DIRS}
        "${VSL_HOME}/include" # ~/repos/AHO/VSLi
        "${AHO_HOME}/include"
        ${GLFW_INCLUDE_DIRS}
)

target_link_directories(AHOLi
        PRIVATE
        lib
        ${GLFW_LIBRARY_DIRS}
)

target_link_libraries(AHOLi
        PUBLIC saitekikuukan::saitekikuukan
        PRIVATE
        ${Boost_LIBRARIES}
        ${GLFW_LIBRARIES}
        VSL
)

if(APPLE)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT DEFINED CMAKE_OBJC_COMPILER)
        find_program(CMAKE_OBJC_COMPILER NAMES clang REQUIRED)
    endif()
    enable_language(OBJC)
    target_sources(AHOLi PRIVATE AHO/src/window_macos.m)
    target_link_libraries(AHOLi PRIVATE "-framework CoreGraphics")
endif()

set_target_properties(AHOLi
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY
        ${CMAKE_BINARY_DIR}/lib)

install(TARGETS AHOLi
        EXPORT AHOTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/AHOLi/AHO/include/
        DESTINATION include
)

install(EXPORT AHOTargets
        FILE AHOTargets.cmake
        NAMESPACE AHO::
        DESTINATION share/AHO/cmake
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
        ${CMAKE_BINARY_DIR}/share/AHO/cmake/AHOConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
        ${CMAKE_SOURCE_DIR}/AHOLi/AHOConfig.cmake.in
        ${CMAKE_BINARY_DIR}/share/AHO/cmake/AHOConfig.cmake
        INSTALL_DESTINATION share/AHO/cmake
)

install(
        FILES
        ${CMAKE_BINARY_DIR}/share/AHO/cmake/AHOConfig.cmake
        ${CMAKE_BINARY_DIR}/share/AHO/cmake/AHOConfigVersion.cmake
        DESTINATION share/AHO/cmake
)

include(cmake/ExperimentalMesh.cmake)

option(AHO_BUILD_TYPED_EXAMPLE "Build the Typed graphics example" OFF)
if(AHO_BUILD_TYPED_EXAMPLE)
    add_subdirectory(AHO/examples/typed)
endif()

if (BUILD_TESTING)
    add_subdirectory(AHO/test)
endif ()

include(cmake/MSDF.cmake)

option(AHO_BUILD_TRIANGLE_EXAMPLE "Build the Typed aho::draw example" OFF)
if(AHO_BUILD_TRIANGLE_EXAMPLE)
    add_executable(aho-triangle-example AHO/examples/triangle.cpp)
    target_include_directories(aho-triangle-example PRIVATE "${AHO_HOME}/include" "${VSL_HOME}/include")
    target_link_directories(aho-triangle-example PRIVATE ${GLFW_LIBRARY_DIRS})
    target_link_libraries(aho-triangle-example PRIVATE AHOLi VSL)
endif()

option(AHO_BUILD_GLTF_EXAMPLE "Build the CPU glTF/GLB inspection example" OFF)
if(AHO_BUILD_GLTF_EXAMPLE)
    add_executable(aho-gltf-inspect "${CMAKE_CURRENT_SOURCE_DIR}/../examples/gltf_inspect.cpp")
    target_include_directories(aho-gltf-inspect PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/AHO/include" "${VSL_HOME}/include")
    target_link_directories(aho-gltf-inspect PRIVATE ${GLFW_LIBRARY_DIRS})
    target_link_libraries(aho-gltf-inspect PRIVATE AHOLi)
endif()

option(AHO_BUILD_GLTF_VIEWER "Build the animated glTF window example" OFF)
if(AHO_BUILD_GLTF_VIEWER)
    find_program(AHO_VIEWER_GLSLC NAMES glslc HINTS "$ENV{VULKAN_SDK}/bin" REQUIRED)
    set(AHO_VIEWER_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/gltf-viewer-shaders")
    set(AHO_VIEWER_SHADER_OUTPUTS)
    foreach(stage vert frag)
        set(source "${CMAKE_CURRENT_SOURCE_DIR}/../examples/shaders/gltf_viewer.${stage}")
        set(output "${AHO_VIEWER_SHADER_DIR}/gltf_viewer.${stage}.spv")
        add_custom_command(OUTPUT "${output}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${AHO_VIEWER_SHADER_DIR}"
                COMMAND "${AHO_VIEWER_GLSLC}" "${source}" -o "${output}"
                DEPENDS "${source}" VERBATIM)
        list(APPEND AHO_VIEWER_SHADER_OUTPUTS "${output}")
    endforeach()
    add_custom_target(aho-gltf-viewer-shaders DEPENDS ${AHO_VIEWER_SHADER_OUTPUTS})
    add_executable(aho-gltf-viewer "${CMAKE_CURRENT_SOURCE_DIR}/../examples/gltf_viewer.cpp")
    add_dependencies(aho-gltf-viewer aho-gltf-viewer-shaders)
    target_include_directories(aho-gltf-viewer PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/AHO/include" "${VSL_HOME}/include"
            ${Vulkan_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS})
    target_compile_definitions(aho-gltf-viewer PRIVATE
            AHO_GLTF_VIEWER_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../examples/assets/fox"
            AHO_GLTF_VIEWER_SHADER_DIR="${AHO_VIEWER_SHADER_DIR}")
    target_link_directories(aho-gltf-viewer PRIVATE ${GLFW_LIBRARY_DIRS})
    target_link_libraries(aho-gltf-viewer PRIVATE AHOLi VSL)
    if(BUILD_TESTING)
        add_test(NAME AHO.GltfViewer.WindowIntegration
                COMMAND aho-gltf-viewer --verify "${CMAKE_CURRENT_BINARY_DIR}/gltf-viewer-captures")
        set_tests_properties(AHO.GltfViewer.WindowIntegration PROPERTIES LABELS gpu RESOURCE_LOCK aho_graphics TIMEOUT 90)
    endif()
endif()
