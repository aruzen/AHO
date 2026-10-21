find_package(glm CONFIG REQUIRED)
find_program(SAMPLE_GLSLANG glslangValidator HINTS "$ENV{VULKAN_SDK}/bin" REQUIRED)
set(SAMPLE_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
foreach(stage vert frag)
    set(output "${SAMPLE_SHADER_DIR}/world.${stage}.spv")
    add_custom_command(OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SAMPLE_SHADER_DIR}"
        COMMAND "${SAMPLE_GLSLANG}" -V "${CMAKE_CURRENT_SOURCE_DIR}/shaders/world.${stage}" -o "${output}"
        DEPENDS "shaders/world.${stage}" VERBATIM)
    list(APPEND SAMPLE_SHADERS "${output}")
endforeach()
add_executable(aho-wander src/main.cpp src/world.cpp ${SAMPLE_SHADERS})
target_include_directories(aho-wander PRIVATE
    "${PROJECT_SOURCE_DIR}/AHOLi/AHO/include" "${PROJECT_SOURCE_DIR}/VSLi/VSL/include"
    ${Vulkan_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS})
target_link_directories(aho-wander PRIVATE ${GLFW_LIBRARY_DIRS})
target_link_libraries(aho-wander PRIVATE AHOLi VSL glm::glm ${GLFW_LIBRARIES})
target_compile_definitions(aho-wander PRIVATE AHO_WANDER_SHADER_DIR="${SAMPLE_SHADER_DIR}")
if(BUILD_TESTING)
    add_test(NAME SampleGame.World COMMAND aho-wander --self-test)
endif()
