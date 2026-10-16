execute_process(COMMAND "${CMAKE_COMMAND}" --build "${TEST_BUILD_DIR}" --target "${TEST_TARGET}" --config "${TEST_CONFIGURATION}"
        RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_errors TIMEOUT 70)
if(NOT build_result STREQUAL "0")
    message(FATAL_ERROR "The public API must build and link successfully.\n${build_output}${build_errors}")
endif()
if(TEST_RUN_PROGRAM)
    execute_process(COMMAND "${TEST_PROGRAM}" --gtest_color=no
            RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output ERROR_VARIABLE run_errors TIMEOUT 15)
    if(NOT run_result STREQUAL "0")
        message(FATAL_ERROR "The public API's behavior must satisfy its contract.\n${run_output}${run_errors}")
    endif()
endif()
