add_custom_target(vsl-tests)

file(GLOB_RECURSE VSL_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(FILTER VSL_TEST_SOURCES EXCLUDE REGEX "/contracts/")
if (VSL_TEST_SOURCES)
    find_program(VSL_TEST_GLSLC NAMES glslc HINTS "$ENV{VULKAN_SDK}/bin" REQUIRED)
    set(VSL_TEST_PUBLIC_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
    configure_file(support/config.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/vsl_test_config.hpp" @ONLY)
    set(VSL_TEST_SHADER_OUTPUTS)
    foreach(VSL_TEST_SHADER triangle.vert triangle.frag vertices.vert fill.comp images.comp reflection.vert reflection_sparse.vert reflection_matrix.vert reflection_sparse_sets.comp reflection_numeric_widths.vert reflection_fragment_inputs.frag picking.frag
            descriptor_arrays.comp descriptor_multidimensional.comp descriptor_runtime.comp
            descriptor_specialization.comp descriptor_specialization_expression.comp descriptor_count_overflow.comp
            reflection_stage.geom reflection_stage.tesc reflection_stage.tese reflection_ext.task reflection_ext.mesh reflection_nv.task reflection_nv.mesh reflection_stage.rgen reflection_stage.rmiss reflection_stage.rint reflection_stage.rahit reflection_stage.rchit reflection_stage.rcall
            reflection_separate_images.comp reflection_separate_arrays.comp reflection_separate_texel.comp
            reflection_texel_buffers.comp reflection_texel_arrays.comp
            reflection_input_attachment.frag reflection_input_arrays.frag
            reflection_vertex_arrays.vert reflection_vertex_array_shapes.vert
            typed_block_layout.comp typed_block_arrays.comp typed_block_numeric.comp typed_push.vert typed_push_overlap.vert typed_push_overlap.frag typed_push_distinct.frag typed_push_partial.frag typed_push_resources.vert
            typed_descriptors.vert typed_descriptors_relocated.vert typed_descriptors.comp typed_texture.frag typed_components.vert typed_numeric.vert typed_wide.vert typed_runtime_descriptors.vert)
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
    find_program(VSL_TEST_SPIRV_AS NAMES spirv-as HINTS "$ENV{VULKAN_SDK}/bin" REQUIRED)
    set(typed_entry_points "${CMAKE_CURRENT_BINARY_DIR}/shaders/typed_entry_points.spv")
    add_custom_command(OUTPUT "${typed_entry_points}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND "${VSL_TEST_SPIRV_AS}" --target-env vulkan1.0 "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/typed_entry_points.spvasm" -o "${typed_entry_points}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/typed_entry_points.spvasm" VERBATIM)
    list(APPEND VSL_TEST_SHADER_OUTPUTS "${typed_entry_points}")
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
    add_library(vsl-typed-style-contract OBJECT EXCLUDE_FROM_ALL contracts/typed_pipeline.cpp)
    target_compile_definitions(vsl-typed-style-contract PRIVATE VSL_TYPED_CASE=0)
    target_include_directories(vsl-typed-style-contract PRIVATE "$<TARGET_PROPERTY:vsl-test,INCLUDE_DIRECTORIES>")
    add_dependencies(vsl-tests vsl-test)
    gtest_discover_tests(vsl-test TEST_PREFIX "VSL." DISCOVERY_MODE PRE_TEST NO_PRETTY_VALUES PROPERTIES TIMEOUT 60)

    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
        CONTENT "set(TEST_COMPILER [==[${CMAKE_CXX_COMPILER}]==])\nset(TEST_COMPILER_ID [==[${CMAKE_CXX_COMPILER_ID}]==])\nset(TEST_FRONTEND [==[${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}]==])\nset(TEST_INCLUDE_DIRS [==[$<TARGET_PROPERTY:vsl-test,INCLUDE_DIRECTORIES>]==])\nset(TEST_FLAGS [==[${CMAKE_CXX_FLAGS}]==])\nset(TEST_SYSROOT [==[${CMAKE_OSX_SYSROOT}]==])\nset(TEST_ARCHITECTURES [==[${CMAKE_OSX_ARCHITECTURES}]==])\n")
    set(typed_contract_names Valid WrongVertexBinding MissingBuffer WrongRecord DuplicateBinding DuplicateLocation
        ExtentOverflow StrideTooSmall ImplicitRecordOffset WideLocationOverlap UndefinedFormat
        MemberStorageOverflow WrongMemberRecord RecordLocationOverflow UnsupportedMember UnsupportedMemberMatrix)
    foreach(case RANGE 0 15)
        list(GET typed_contract_names ${case} name)
        add_test(NAME "VSL.TypedPipeline.Compile.${name}" COMMAND "${CMAKE_COMMAND}"
            "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
            "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/typed_pipeline.cpp"
            "-DTEST_CASE=${case}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
        set_tests_properties("VSL.TypedPipeline.Compile.${name}" PROPERTIES LABELS compile TIMEOUT 60)
    endforeach()
    set(typed_descriptor_names Valid WrongPayload WrongKind WrongArrayCount UnknownBinding WrongSet MissingSet
        MissingBind DuplicateBinding DuplicateSet ZeroArrayCount RawSet RawBuffer)
    foreach(case RANGE 0 12)
        list(GET typed_descriptor_names ${case} name)
        add_test(NAME "VSL.TypedDescriptor.Compile.${name}" COMMAND "${CMAKE_COMMAND}"
            "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
            "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/typed_descriptors.cpp"
            "-DTEST_CASE=${case}" "-DTEST_EXPECTED=write|bind_resources|draw|Duplicate typed|Invalid typed"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
        set_tests_properties("VSL.TypedDescriptor.Compile.${name}" PROPERTIES LABELS compile TIMEOUT 60)
    endforeach()
    add_library(vsl-descriptor-style-contract OBJECT EXCLUDE_FROM_ALL contracts/typed_descriptors.cpp)
    target_compile_definitions(vsl-descriptor-style-contract PRIVATE VSL_TYPED_CASE=0)
    target_include_directories(vsl-descriptor-style-contract PRIVATE "$<TARGET_PROPERTY:vsl-test,INCLUDE_DIRECTORIES>")
    set(typed_push_names Valid WrongPayload MissingPayload ExtraPayload MissingDrawPush WrongSchema Overlap
        MisalignedOffset MisalignedSize EmptyStages MissingDispatchPush WrongDispatchSchema)
    foreach(case RANGE 0 11)
        list(GET typed_push_names ${case} name)
        add_test(NAME "VSL.TypedPush.Compile.${name}" COMMAND "${CMAKE_COMMAND}"
            "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
            "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/typed_push_constants.cpp"
            "-DTEST_CASE=${case}" "-DTEST_EXPECTED=PushValues|push_constants|draw|dispatch|Overlapping typed push|Invalid typed push|Typed push stages"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
        set_tests_properties("VSL.TypedPush.Compile.${name}" PROPERTIES LABELS compile TIMEOUT 60)
    endforeach()
    add_library(vsl-push-style-contract OBJECT EXCLUDE_FROM_ALL contracts/typed_push_constants.cpp)
    target_compile_definitions(vsl-push-style-contract PRIVATE VSL_TYPED_CASE=0)
    target_include_directories(vsl-push-style-contract PRIVATE "$<TARGET_PROPERTY:vsl-test,INCLUDE_DIRECTORIES>")
    set(typed_block_names Valid MissingDescriptorMetadata MissingPushMetadata WrongMemberRecord MemberStorageOverflow
        MemberOffsetOverflow InvalidMatrixDimensions IntegerMatrix InvalidMatrixStride ExtentOverflow ZeroArrayCapacity WrongBufferPayload InvalidFormatExtent)
    foreach(case RANGE 0 12)
        list(GET typed_block_names ${case} name)
        add_test(NAME "VSL.TypedBlock.Compile.${name}" COMMAND "${CMAKE_COMMAND}"
            "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
            "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/typed_block_layout.cpp"
            "-DTEST_CASE=${case}" "-DTEST_EXPECTED=Typed block|Typed descriptor|Typed push|TypedUniformBuffer|TypedResourceBuffer|constant expression"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
        set_tests_properties("VSL.TypedBlock.Compile.${name}" PROPERTIES LABELS compile TIMEOUT 60)
    endforeach()
    add_library(vsl-block-style-contract OBJECT EXCLUDE_FROM_ALL contracts/typed_block_layout.cpp)
    target_compile_definitions(vsl-block-style-contract PRIVATE VSL_TYPED_CASE=0)
    target_include_directories(vsl-block-style-contract PRIVATE "$<TARGET_PROPERTY:vsl-test,INCLUDE_DIRECTORIES>")
endif ()
