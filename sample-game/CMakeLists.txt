find_package(glm CONFIG REQUIRED)
include(cmake/MoltenVK.cmake)
find_program(SAMPLE_GLSLANG glslangValidator HINTS "$ENV{VULKAN_SDK}/bin" REQUIRED)
set(SAMPLE_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
foreach(shader world.vert world.frag model.vert model.frag)
    set(output "${SAMPLE_SHADER_DIR}/${shader}.spv")
    add_custom_command(OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SAMPLE_SHADER_DIR}"
        COMMAND "${SAMPLE_GLSLANG}" -V "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}" -o "${output}"
        DEPENDS "shaders/${shader}" VERBATIM)
    list(APPEND SAMPLE_SHADERS "${output}")
endforeach()
add_executable(aho-wander src/main.cpp src/world.cpp ${SAMPLE_SHADERS})
target_include_directories(aho-wander PRIVATE
    "${PROJECT_SOURCE_DIR}/AHOLi/AHO/include" "${PROJECT_SOURCE_DIR}/VSLi/VSL/include"
    ${Vulkan_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS})
target_link_directories(aho-wander PRIVATE ${GLFW_LIBRARY_DIRS})
target_link_libraries(aho-wander PRIVATE AHOLi VSL glm::glm ${GLFW_LIBRARIES})
target_compile_definitions(aho-wander PRIVATE AHO_WANDER_SHADER_DIR="${SAMPLE_SHADER_DIR}")
target_compile_definitions(aho-wander PRIVATE AHO_WANDER_MODEL="${PROJECT_SOURCE_DIR}/examples/assets/fox/Fox.glb")
if(APPLE AND AHO_WANDER_LOCAL_MOLTENVK)
    target_compile_definitions(aho-wander PRIVATE AHO_WANDER_ICD_FILE="${AHO_WANDER_ICD_FILE}")
endif()
if(BUILD_TESTING)
    add_test(NAME SampleGame.World COMMAND aho-wander --self-test)
    add_test(NAME SampleGame.WindowIntegration COMMAND aho-wander --verify "${CMAKE_CURRENT_BINARY_DIR}/captures")
    set_tests_properties(SampleGame.World PROPERTIES LABELS cpu TIMEOUT 20)
    set_tests_properties(SampleGame.WindowIntegration PROPERTIES LABELS gpu RESOURCE_LOCK aho_graphics TIMEOUT 180)
    # An existing executable cannot become an output directory. Must exit 1 cleanly, without aborting at teardown.
    add_test(NAME SampleGame.ErrorCleanup COMMAND aho-wander --verify "$<TARGET_FILE:aho-wander>")
    set_tests_properties(SampleGame.ErrorCleanup PROPERTIES LABELS gpu RESOURCE_LOCK aho_graphics TIMEOUT 60 WILL_FAIL TRUE)
    if(APPLE AND AHO_WANDER_LOCAL_MOLTENVK)
        set_tests_properties(SampleGame.WindowIntegration SampleGame.ErrorCleanup PROPERTIES
            ENVIRONMENT "VK_ICD_FILENAMES=${AHO_WANDER_ICD_FILE};VK_DRIVER_FILES=${AHO_WANDER_ICD_FILE}")
        if(AHO_BUILD_GLTF_VIEWER)
            set_tests_properties(AHO.GltfViewer.WindowIntegration DIRECTORY "${PROJECT_SOURCE_DIR}/AHOLi" PROPERTIES
                ENVIRONMENT "VK_ICD_FILENAMES=${AHO_WANDER_ICD_FILE};VK_DRIVER_FILES=${AHO_WANDER_ICD_FILE}")
        endif()
    endif()
endif()
