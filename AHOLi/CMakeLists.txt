cmake_minimum_required(VERSION 3.20)
project(AHOLi VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
        AHO/src/x/drawer/draw_list.cpp
        AHO/src/image_readback.cpp
        AHO/src/x/screenshot.cpp
        AHO/src/x/drawer/renderer2d.cpp
        AHO/src/x/command/draw2d.cpp
        AHO/src/x/drawer/renderer2d_impl.hpp
        AHO/src/engine.cpp
        AHO/src/resource/image.cpp
        AHO/src/resource/buffer.cpp
        AHO/src/engine/standard_engine.cpp
        AHO/src/engine/graphical_engine.cpp

        AHO/include/AHO/image_readback.hpp
        AHO/include/AHO/x/screenshot.hpp
        AHO/include/AHO/x/drawer/draw_list.hpp
        AHO/include/AHO/x/drawer/renderer2d.hpp
        AHO/include/AHO/x/command/draw2d.hpp
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
        PRIVATE
        ${Boost_LIBRARIES}
        ${GLFW_LIBRARIES}
        VSL
)

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

find_program(AHO_DRAW_GLSLC NAMES glslc HINTS "$ENV{VULKAN_SDK}/bin" REQUIRED)
set(AHO_DRAW_SHADER_OUTPUTS)
foreach(shader draw2d.vert draw2d.frag)
    set(output "${CMAKE_CURRENT_BINARY_DIR}/shaders/drawer/${shader}.spv")
    add_custom_command(OUTPUT "${output}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders/drawer"
            COMMAND "${AHO_DRAW_GLSLC}" "${CMAKE_CURRENT_SOURCE_DIR}/AHO/shaders/drawer/${shader}" -o "${output}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/AHO/shaders/drawer/${shader}"
            VERBATIM)
    list(APPEND AHO_DRAW_SHADER_OUTPUTS "${output}")
endforeach()
add_custom_target(aho-draw-shaders DEPENDS ${AHO_DRAW_SHADER_OUTPUTS})
add_dependencies(AHOLi aho-draw-shaders)
install(FILES ${AHO_DRAW_SHADER_OUTPUTS} DESTINATION share/AHO/shaders/drawer)
