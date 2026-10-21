cmake_minimum_required(VERSION 3.20)
project(AHOLi VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(cmake/Stb.cmake)

include(cmake/TinyGltf.cmake)

add_library(AHOLi
        AHO/src/pipeline/layouts.cpp
        AHO/src/pipeline/pipeline.cpp
        AHO/src/window.cpp
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
        AHO/src/x/scene/mesh_renderer.cpp
        AHO/src/x/scene/gltf_renderer.cpp
        AHO/src/engine.cpp
        AHO/src/resource/image.cpp
        AHO/src/resource/buffer.cpp
        AHO/src/engine/standard_engine.cpp
        AHO/src/engine/graphical_engine.cpp

        AHO/include/AHO/image_readback.hpp
        AHO/include/AHO/x/resource/gltf.hpp
        AHO/include/AHO/x/resource/shader.hpp
        AHO/include/AHO/x/screenshot.hpp
        AHO/include/aho/engine.hpp
        AHO/include/aho/concepts.hpp
        AHO/include/aho/pipeline/layouts.hpp
        AHO/include/aho/pipeline/pipeline.hpp
        AHO/include/aho/drawer/standard_drawer.hpp
        AHO/include/aho/core/collision.hpp
        AHO/include/aho/core/index.hpp
        AHO/include/aho/core/vector.hpp
        AHO/include/aho/core/triangle.hpp
        AHO/include/aho/core/point.hpp
        AHO/include/aho/core/polygon.hpp
        AHO/include/aho/core/math/angle.hpp
        AHO/include/aho/core/math/mat.hpp
        AHO/include/aho/core/math/coordinate.hpp
        AHO/include/aho/core/color.hpp
        AHO/include/aho/core/line.hpp
        AHO/include/aho/core/ov.hpp
        AHO/include/aho/io/keyboard.hpp
        AHO/include/aho/io/key.hpp
        AHO/include/aho/io/input_manager.hpp
        AHO/include/aho/io/mouse.hpp
        AHO/include/aho/io/input.hpp
        AHO/include/aho/drawer.hpp
        AHO/include/aho/utils/polygon.hpp
        AHO/include/aho/window.hpp
        AHO/include/aho/object/object.hpp
        AHO/include/aho/resource/audio.hpp
        AHO/include/aho/resource/buffer.hpp
        AHO/include/aho/resource/object_hitbox.hpp
        AHO/include/aho/resource/image.hpp
        AHO/include/aho/resource/object_model.hpp
        AHO/include/aho/engine/graphical_engine.hpp
        AHO/include/aho/engine/standard_engine.hpp
        AHO/include/aho/define.hpp
        AHO/src/drawer/draw_phase.cpp
)

target_include_directories(AHOLi SYSTEM PRIVATE "${AHO_TINYGLTF_INCLUDE_DIR}")

target_include_directories(AHOLi
        PRIVATE
        ${Boost_INCLUDE_DIRS}
        "${VSL_HOME}/include" # ~/repos/AHO/VSLi
        "${AHO_HOME}/include"
        ${GLFW_INCLUDE_DIRS}
)
target_include_directories(AHOLi SYSTEM PRIVATE "${AHO_STB_INCLUDE_DIR}")

target_link_directories(AHOLi
        PRIVATE
        lib
        ${GLFW_LIBRARY_DIRS}
)

target_link_libraries(AHOLi
        PRIVATE
        ${Boost_LIBRARIES}
        ${GLFW_LIBRARIES}
        VSL
)
find_package(glm CONFIG REQUIRED)
target_link_libraries(AHOLi PUBLIC glm::glm)

if(APPLE)
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

if (BUILD_TESTING)
    add_subdirectory(AHO/test)
endif ()

option(AHO_BUILD_GLTF_EXAMPLE "Build the CPU glTF/GLB inspection example" OFF)
if(AHO_BUILD_GLTF_EXAMPLE)
    add_executable(aho-gltf-inspect "${CMAKE_CURRENT_SOURCE_DIR}/../examples/gltf_inspect.cpp")
    target_include_directories(aho-gltf-inspect PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/AHO/include")
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
