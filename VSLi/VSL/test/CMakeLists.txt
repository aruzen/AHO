add_custom_target(vsl-tests)

file(GLOB_RECURSE VSL_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
if (VSL_TEST_SOURCES)
    find_program(VSL_TEST_GLSLC NAMES glslc HINTS "$ENV{VULKAN_SDK}/bin" REQUIRED)
    set(VSL_TEST_PUBLIC_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
    configure_file(support/config.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/vsl_test_config.hpp" @ONLY)
    set(VSL_TEST_SHADER_OUTPUTS)
    foreach(VSL_TEST_SHADER triangle.vert triangle.frag vertices.vert fill.comp images.comp reflection.vert reflection_sparse.vert reflection_matrix.vert reflection_sparse_sets.comp reflection_numeric_widths.vert reflection_fragment_inputs.frag picking.frag
            descriptor_arrays.comp descriptor_multidimensional.comp descriptor_runtime.comp
            descriptor_specialization.comp descriptor_specialization_expression.comp descriptor_count_overflow.comp
            reflection_stage.geom reflection_stage.tesc reflection_stage.tese reflection_ext.task reflection_ext.mesh reflection_nv.task reflection_nv.mesh reflection_stage.rgen reflection_stage.rmiss reflection_stage.rint reflection_stage.rahit reflection_stage.rchit reflection_stage.rcall)
        set(VSL_TEST_SHADER_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/${VSL_TEST_SHADER}.spv")
        set(VSL_TEST_SHADER_OPTIONS)
        if (VSL_TEST_SHADER MATCHES "\\.(rgen|rmiss|rint|rahit|rchit|rcall|task|mesh)$")
            list(APPEND VSL_TEST_SHADER_OPTIONS --target-env=vulkan1.2)
        endif()
        add_custom_command(OUTPUT "${VSL_TEST_SHADER_OUTPUT}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
                COMMAND "${VSL_TEST_GLSLC}" ${VSL_TEST_SHADER_OPTIONS} "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/${VSL_TEST_SHADER}" -o "${VSL_TEST_SHADER_OUTPUT}"
                DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/${VSL_TEST_SHADER}"
                VERBATIM)
        list(APPEND VSL_TEST_SHADER_OUTPUTS "${VSL_TEST_SHADER_OUTPUT}")
    endforeach()
    add_custom_target(vsl-test-shaders DEPENDS ${VSL_TEST_SHADER_OUTPUTS})
    add_executable(vsl-test ${VSL_TEST_SOURCES})
    add_dependencies(vsl-test vsl-test-shaders)
    target_compile_definitions(vsl-test PRIVATE
            VSL_TEST_SHADER_DIR="${CMAKE_CURRENT_BINARY_DIR}/shaders")
    target_include_directories(vsl-test PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../include"
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_BINARY_DIR}"
            "$<TARGET_PROPERTY:VSL,INCLUDE_DIRECTORIES>"
    )
    target_link_directories(vsl-test PRIVATE ${GLFW_LIBRARY_DIRS})
    target_link_libraries(vsl-test PRIVATE VSL GTest::gtest_main)
    add_dependencies(vsl-tests vsl-test)
    gtest_discover_tests(vsl-test TEST_PREFIX "VSL." DISCOVERY_MODE PRE_TEST NO_PRETTY_VALUES PROPERTIES TIMEOUT 60)
endif ()
