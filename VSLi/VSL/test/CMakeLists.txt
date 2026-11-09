add_custom_target(vsl-tests)
cmake_language(DEFER CALL aho_copy_windows_runtime)

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
            typed_bundle.comp typed_arena_images.comp typed_frame.comp typed_batch.comp typed_batch_red.vert typed_block_layout.comp typed_block_arrays.comp typed_block_numeric.comp typed_push.vert typed_push_overlap.vert typed_push_overlap.frag typed_push_distinct.frag typed_push_partial.frag typed_push_resources.vert
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
            "${VSL_HOME}/src/private"
    )
    target_link_directories(vsl-test PRIVATE ${GLFW_LIBRARY_DIRS})
    # 寸法probeと同じSDKを先にし、GLFWのprefix内にある別版Vulkanを混在させない。
    target_include_directories(vsl-test SYSTEM BEFORE PRIVATE ${Vulkan_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS})
    target_link_libraries(vsl-test PRIVATE VSL GTest::gtest_main)
    add_library(vsl-typed-style-contract OBJECT EXCLUDE_FROM_ALL contracts/typed_pipeline.cpp)
    target_compile_definitions(vsl-typed-style-contract PRIVATE VSL_TYPED_CASE=0)
    target_include_directories(vsl-typed-style-contract PRIVATE "$<TARGET_PROPERTY:vsl-test,INCLUDE_DIRECTORIES>")
    add_dependencies(vsl-tests vsl-test)
    add_library(vsl-resource-plan-style-contract OBJECT EXCLUDE_FROM_ALL contracts/resource_plan.cpp)
    target_link_libraries(vsl-resource-plan-style-contract PRIVATE VSL)
    set(resource_plan_contract_names Valid DuplicateResource InvalidLifetime InvalidKind Capacity TemporaryPlacements ConstTemporaryPlacements TemporaryArenaBorrow ConstTemporaryArenaBorrow)
    foreach(case RANGE 0 8)
        list(GET resource_plan_contract_names ${case} name)
        add_test(NAME "VSL.ResourcePlan.Compile.${name}" COMMAND "${CMAKE_COMMAND}"
            "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
            "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/resource_plan.cpp"
            "-DTEST_CASE=${case}" "-DTEST_EXPECTED=constant expression|deleted"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
        set_tests_properties("VSL.ResourcePlan.Compile.${name}" PROPERTIES LABELS compile TIMEOUT 60)
    endforeach()
    add_library(vsl-arena-style-contract OBJECT EXCLUDE_FROM_ALL contracts/typed_buffer_arena.cpp)
    target_link_libraries(vsl-arena-style-contract PRIVATE VSL)
    set(arena_contract_names Valid WrongPayload WrongDestination WrongUsage UnknownResource TemporaryBorrow DuplicateResource
        MixedLifetime EscapingRead DuplicateBinding UnknownBinding UnknownBindingResource WrongReadPayload TypedTemporaryStorage ConstTypedTemporaryStorage)
    foreach(case RANGE 0 14)
        list(GET arena_contract_names ${case} name)
        add_test(NAME "VSL.TypedArena.Compile.${name}" COMMAND "${CMAKE_COMMAND}"
            "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
            "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/typed_buffer_arena.cpp"
            "-DTEST_CASE=${case}" "-DTEST_EXPECTED=copy|copy_to|constant expression|deleted|Duplicate buffer|Unknown buffer|lifetime scope|read_bytes|read_into"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
        set_tests_properties("VSL.TypedArena.Compile.${name}" PROPERTIES LABELS compile TIMEOUT 60)
    endforeach()
    gtest_discover_tests(vsl-test TEST_PREFIX "VSL." DISCOVERY_MODE PRE_TEST NO_PRETTY_VALUES PROPERTIES TIMEOUT 60)

    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
        CONTENT "set(TEST_COMPILER [==[${CMAKE_CXX_COMPILER}]==])\nset(TEST_COMPILER_ID [==[${CMAKE_CXX_COMPILER_ID}]==])\nset(TEST_FRONTEND [==[${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}]==])\nset(TEST_INCLUDE_DIRS [==[$<TARGET_PROPERTY:vsl-test,INCLUDE_DIRECTORIES>]==])\nset(TEST_DEFINITIONS [==[$<TARGET_PROPERTY:VSL,INTERFACE_COMPILE_DEFINITIONS>]==])\nset(TEST_FLAGS [==[${CMAKE_CXX_FLAGS}]==])\nset(TEST_SYSROOT [==[${CMAKE_OSX_SYSROOT}]==])\nset(TEST_ARCHITECTURES [==[${CMAKE_OSX_ARCHITECTURES}]==])\n")
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
    set(typed_index_names Valid Uint8Index SignedIndex Uint64Index RawBuffer MissingResources MissingPush)
    foreach(case RANGE 0 6)
        list(GET typed_index_names ${case} name)
        add_test(NAME "VSL.TypedIndex.Compile.${name}" COMMAND "${CMAKE_COMMAND}"
            "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
            "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/typed_index_buffer.cpp"
            "-DTEST_CASE=${case}" "-DTEST_EXPECTED=index_element|draw_indexed"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
        set_tests_properties("VSL.TypedIndex.Compile.${name}" PROPERTIES LABELS compile TIMEOUT 60)
    endforeach()
    add_library(vsl-index-style-contract OBJECT EXCLUDE_FROM_ALL contracts/typed_index_buffer.cpp)
    target_compile_definitions(vsl-index-style-contract PRIVATE VSL_TYPED_CASE=0)
    target_include_directories(vsl-index-style-contract PRIVATE "$<TARGET_PROPERTY:vsl-test,INCLUDE_DIRECTORIES>")
    set(typed_view_names Valid WrongVertexRecord WrongDescriptorKind WrongDescriptorPayload RawVertexBuffer NegativeSubpass TemporaryBackend ConstTemporaryBackend InvalidStoragePolicy)
    foreach(case RANGE 0 8)
        list(GET typed_view_names ${case} name)
        add_test(NAME "VSL.TypedView.Compile.${name}" COMMAND "${CMAKE_COMMAND}"
            "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
            "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/typed_buffer_view.cpp"
            "-DTEST_CASE=${case}" "-DTEST_EXPECTED=bind_vertices|write|narrowed|narrowing|deleted|constraints|constructor"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
        set_tests_properties("VSL.TypedView.Compile.${name}" PROPERTIES LABELS compile TIMEOUT 60)
    endforeach()
    add_library(vsl-view-style-contract OBJECT EXCLUDE_FROM_ALL contracts/typed_buffer_view.cpp)
    target_compile_definitions(vsl-view-style-contract PRIVATE VSL_TYPED_CASE=0)
    target_include_directories(vsl-view-style-contract PRIVATE "$<TARGET_PROPERTY:vsl-test,INCLUDE_DIRECTORIES>")
    set(typed_batch_names Valid Empty MixedGraphicsCompute RawCommand InvalidCommand UnfinishedBinding)
    foreach(case RANGE 0 5)
        list(GET typed_batch_names ${case} name)
        add_test(NAME "VSL.TypedBatch.Compile.${name}" COMMAND "${CMAKE_COMMAND}"
            "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
            "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/typed_batch.cpp"
            "-DTEST_CASE=${case}" "-DTEST_EXPECTED=Typed batch requires"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
        set_tests_properties("VSL.TypedBatch.Compile.${name}" PROPERTIES LABELS compile TIMEOUT 60)
    endforeach()
    add_library(vsl-batch-style-contract OBJECT EXCLUDE_FROM_ALL contracts/typed_batch.cpp)
    target_compile_definitions(vsl-batch-style-contract PRIVATE VSL_TYPED_CASE=0)
    target_include_directories(vsl-batch-style-contract PRIVATE "$<TARGET_PROPERTY:vsl-test,INCLUDE_DIRECTORIES>")
    set(typed_frame_names Valid RawSlot WrongFactory WrongUpdate NonVoidUpdate MutableGet Copy)
    foreach(case RANGE 0 6)
        list(GET typed_frame_names ${case} name)
        add_test(NAME "VSL.TypedFrame.Compile.${name}" COMMAND "${CMAKE_COMMAND}"
            "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
            "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/typed_frame_resources.cpp"
            "-DTEST_CASE=${case}" "-DTEST_EXPECTED=get|TypedFrameResources|update|read-only|const-qualified|deleted"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
        set_tests_properties("VSL.TypedFrame.Compile.${name}" PROPERTIES LABELS compile TIMEOUT 60)
    endforeach()
    add_library(vsl-frame-style-contract OBJECT EXCLUDE_FROM_ALL contracts/typed_frame_resources.cpp)
    target_compile_definitions(vsl-frame-style-contract PRIVATE VSL_TYPED_CASE=0)
    target_include_directories(vsl-frame-style-contract PRIVATE "$<TARGET_PROPERTY:vsl-test,INCLUDE_DIRECTORIES>")
    set(typed_allocation_names Valid Missing Duplicate UnknownSet UnknownBinding ImageBuffer ZeroElements InvalidScope FixedRuntimeSize UnknownRuntimeSize InvalidElements BufferImage TextureSampler InvalidSamplerScope ImageArrayCount ExternalImageInfo)
    foreach(case RANGE 0 15)
        list(GET typed_allocation_names ${case} name)
        add_test(NAME "VSL.TypedAllocation.Compile.${name}" COMMAND "${CMAKE_COMMAND}"
            "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
            "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/typed_resource_allocation.cpp"
            "-DTEST_CASE=${case}" "-DTEST_EXPECTED=Typed allocation|typed allocation|set|image"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
        set_tests_properties("VSL.TypedAllocation.Compile.${name}" PROPERTIES LABELS compile TIMEOUT 60)
    endforeach()
    add_library(vsl-allocation-style-contract OBJECT EXCLUDE_FROM_ALL contracts/typed_resource_allocation.cpp)
    target_compile_definitions(vsl-allocation-style-contract PRIVATE VSL_TYPED_CASE=0)
    target_include_directories(vsl-allocation-style-contract PRIVATE "$<TARGET_PROPERTY:vsl-test,INCLUDE_DIRECTORIES>")
    set(typed_bundle_names Valid WrongKind WrongPayload WrongArrayCount MissingInput ExtraInput SharedMutation ConstMutation ArrayBounds UnknownBinding SharedLocal NonVoidUpdate DuplicateInput CopyBundle CopyFrame)
    foreach(case RANGE 0 14)
        list(GET typed_bundle_names ${case} name)
        add_test(NAME "VSL.TypedBundle.Compile.${name}" COMMAND "${CMAKE_COMMAND}"
            "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
            "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/typed_resource_bundle.cpp"
            "-DTEST_CASE=${case}" "-DTEST_EXPECTED=TypedResourceBundle|buffer|copy|update|deleted"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
        set_tests_properties("VSL.TypedBundle.Compile.${name}" PROPERTIES LABELS compile TIMEOUT 60)
        add_test(NAME "VSL.TypedArenaBundle.Compile.${name}" COMMAND "${CMAKE_COMMAND}"
            "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
            "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/typed_resource_bundle.cpp"
            "-DTEST_EXTRA_DEFINITIONS=VSL_TYPED_BUNDLE_ARENA"
            "-DTEST_CASE=${case}" "-DTEST_EXPECTED=TypedResourceBundle|buffer|copy|update|deleted"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
        set_tests_properties("VSL.TypedArenaBundle.Compile.${name}" PROPERTIES LABELS compile TIMEOUT 60)
    endforeach()
    add_library(vsl-bundle-style-contract OBJECT EXCLUDE_FROM_ALL contracts/typed_resource_bundle.cpp)
    add_test(NAME VSL.TypedArenaBundle.Compile.NontrivialView COMMAND "${CMAKE_COMMAND}"
        "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
        "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/typed_resource_bundle.cpp"
        "-DTEST_CASE=15" "-DTEST_EXPECTED=is_trivially_copyable"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
    set_tests_properties(VSL.TypedArenaBundle.Compile.NontrivialView PROPERTIES LABELS compile TIMEOUT 60)
    set(typed_owned_names ImageBuffer SharedFrameImage TemporaryInfos TemporaryBuffers TemporaryPlacements)
    foreach(case RANGE 16 20)
        math(EXPR name_index "${case} - 16")
        list(GET typed_owned_names ${name_index} name)
        foreach(policy Heap Arena)
            set(owned_definitions)
            if(policy STREQUAL "Arena")
                set(owned_definitions VSL_TYPED_BUNDLE_ARENA)
            endif()
            add_test(NAME "VSL.TypedOwned.Compile.${policy}.${name}" COMMAND "${CMAKE_COMMAND}"
                "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
                "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/typed_resource_bundle.cpp"
                "-DTEST_EXTRA_DEFINITIONS=${owned_definitions}" "-DTEST_CASE=${case}" "-DTEST_EXPECTED=buffer|image|deleted"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
            set_tests_properties("VSL.TypedOwned.Compile.${policy}.${name}" PROPERTIES LABELS compile TIMEOUT 60)
        endforeach()
    endforeach()
    target_link_libraries(vsl-bundle-style-contract PRIVATE VSL)
    target_compile_definitions(vsl-bundle-style-contract PRIVATE VSL_TYPED_CASE=0)
    target_include_directories(vsl-bundle-style-contract PRIVATE "$<TARGET_PROPERTY:vsl-test,INCLUDE_DIRECTORIES>")
    set(typed_shared_layout_names Accepted ComputeSchema GraphicsSchema UnknownSet HeapEntry RawInjection)
    foreach(case RANGE 0 5)
        list(GET typed_shared_layout_names ${case} name)
        add_test(NAME "VSL.SharedLayouts.Compile.${name}" COMMAND "${CMAKE_COMMAND}"
            "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
            "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/typed_descriptor_layouts.cpp"
            "-DTEST_CASE=${case}" "-DTEST_EXPECTED=TypedComputePipeline|TypedGraphicsPipeline|layout|PipelineLayout"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
        set_tests_properties("VSL.SharedLayouts.Compile.${name}" PROPERTIES LABELS compile TIMEOUT 60)
    endforeach()
    add_library(vsl-shared-layouts-style-contract OBJECT EXCLUDE_FROM_ALL contracts/typed_descriptor_layouts.cpp)
    target_link_libraries(vsl-shared-layouts-style-contract PRIVATE VSL)
    target_compile_definitions(vsl-shared-layouts-style-contract PRIVATE VSL_TYPED_CASE=0)
    target_include_directories(vsl-shared-layouts-style-contract PRIVATE "$<TARGET_PROPERTY:vsl-test,INCLUDE_DIRECTORIES>")
    add_library(vsl-render-pass-style-contract OBJECT EXCLUDE_FROM_ALL contracts/render_pass.cpp)
    target_link_libraries(vsl-render-pass-style-contract PRIVATE VSL)
    target_compile_definitions(vsl-render-pass-style-contract PRIVATE VSL_TYPED_CASE=0 VSL_RENDER_PASS_FACADE)
    set(render_pass_contract_names DescriptionHeader FacadeHeader ConstTemporaryDescription TemporaryDescription)
    foreach(case RANGE 0 3)
        list(GET render_pass_contract_names ${case} name)
        set(contract_case ${case})
        if(case LESS 2)
            set(contract_case 0)
            if(case EQUAL 1)
                set(extra_definitions VSL_RENDER_PASS_FACADE)
            else()
                set(extra_definitions)
            endif()
        else()
            set(extra_definitions)
        endif()
        add_test(NAME "VSL.RenderPass.Compile.${name}" COMMAND "${CMAKE_COMMAND}"
            "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
            "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/render_pass.cpp"
            "-DTEST_EXTRA_DEFINITIONS=${extra_definitions}"
            "-DTEST_CASE=${contract_case}" "-DTEST_EXPECTED=deleted"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
        set_tests_properties("VSL.RenderPass.Compile.${name}" PROPERTIES LABELS compile TIMEOUT 60)
    endforeach()
    add_library(vsl-graphics-state-style-contract OBJECT EXCLUDE_FROM_ALL contracts/graphics_pipeline_state.cpp)
    target_link_libraries(vsl-graphics-state-style-contract PRIVATE VSL)
    target_compile_definitions(vsl-graphics-state-style-contract PRIVATE VSL_TYPED_CASE=0)
    set(graphics_state_contract_names Valid ShaderSource DescriptorSource PushSource TemporaryEdit ConstTemporaryDefinition TemporaryLayoutState)
    foreach(case RANGE 0 6)
        list(GET graphics_state_contract_names ${case} name)
        add_test(NAME "VSL.GraphicsState.Compile.${name}" COMMAND "${CMAKE_COMMAND}"
            "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
            "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/graphics_pipeline_state.cpp"
            "-DTEST_CASE=${case}" "-DTEST_EXPECTED=GraphicsPipelineState|deleted"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
        set_tests_properties("VSL.GraphicsState.Compile.${name}" PROPERTIES LABELS compile TIMEOUT 60)
    endforeach()
    add_library(vsl-pipeline-style-contract OBJECT EXCLUDE_FROM_ALL contracts/pipeline.cpp)
    target_link_libraries(vsl-pipeline-style-contract PRIVATE VSL)
    add_test(NAME VSL.Pipeline.Compile.ExplicitRuntime COMMAND "${CMAKE_COMMAND}"
        "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
        "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/pipeline.cpp"
        "-DTEST_CASE=0" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
    set_tests_properties(VSL.Pipeline.Compile.ExplicitRuntime PROPERTIES LABELS compile TIMEOUT 60)
    add_library(vsl-frame-buffer-style-contract OBJECT EXCLUDE_FROM_ALL contracts/frame_buffer.cpp)
    target_link_libraries(vsl-frame-buffer-style-contract PRIVATE VSL)
    add_test(NAME VSL.FrameBuffer.Compile.PublicPlacement COMMAND "${CMAKE_COMMAND}"
        "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
        "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/frame_buffer.cpp"
        "-DTEST_CASE=0" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
    set_tests_properties(VSL.FrameBuffer.Compile.PublicPlacement PROPERTIES LABELS compile TIMEOUT 60)
    add_library(vsl-command-arena-style-contract OBJECT EXCLUDE_FROM_ALL contracts/command_arena.cpp)
    target_link_libraries(vsl-command-arena-style-contract PRIVATE VSL)
    add_test(NAME VSL.CommandArena.Compile.PublicPlacement COMMAND "${CMAKE_COMMAND}"
        "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/typed-environment-$<CONFIG>.cmake"
        "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contracts/command_arena.cpp"
        "-DTEST_CASE=0" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTypedContract.cmake")
    set_tests_properties(VSL.CommandArena.Compile.PublicPlacement PROPERTIES LABELS compile TIMEOUT 60)
endif ()
