set(aho_test_config_flags "")
set(aho_test_configurations Debug Release RelWithDebInfo MinSizeRel ${CMAKE_CONFIGURATION_TYPES} ${CMAKE_BUILD_TYPE})
list(REMOVE_DUPLICATES aho_test_configurations)
foreach(configuration IN LISTS aho_test_configurations)
    string(TOUPPER "${configuration}" upper_configuration)
    string(APPEND aho_test_config_flags "$<$<CONFIG:${configuration}>:${CMAKE_CXX_FLAGS_${upper_configuration}}>")
endforeach()
file(GENERATE
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/compile-environment-$<CONFIG>.cmake"
        CONTENT "set(TEST_COMPILER [==[${CMAKE_CXX_COMPILER}]==])\nset(TEST_COMPILER_ID [==[${CMAKE_CXX_COMPILER_ID}]==])\nset(TEST_INCLUDE_DIRS [==[$<TARGET_PROPERTY:aho-test,INCLUDE_DIRECTORIES>]==])\nset(TEST_DEFINITIONS [==[$<TARGET_PROPERTY:aho-test,COMPILE_DEFINITIONS>]==])\nset(TEST_FLAGS [==[${CMAKE_CXX_COMPILER_ARG1} ${CMAKE_CXX_FLAGS} ${aho_test_config_flags}]==])\nset(TEST_COMPILE_OPTIONS [==[$<TARGET_PROPERTY:aho-test,COMPILE_OPTIONS>]==])\nset(TEST_SYSROOT [==[${CMAKE_OSX_SYSROOT}]==])\nset(TEST_ARCHITECTURES [==[${CMAKE_OSX_ARCHITECTURES}]==])\n"
        TARGET aho-test
)

function(aho_add_compile_test name source definition)
    add_test(NAME "AHO.Compile.${name}"
            COMMAND "${CMAKE_COMMAND}"
            "-DTEST_ENVIRONMENT=${CMAKE_CURRENT_BINARY_DIR}/compile-environment-$<CONFIG>.cmake"
            "-DTEST_SOURCE=${source}"
            "-DTEST_EXTRA_DEFINITION=${definition}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunCompileContract.cmake"
    )
    set_tests_properties("AHO.Compile.${name}" PROPERTIES LABELS "AHO;compile" TIMEOUT 60)
endfunction()

function(aho_register_header_tests)
    file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../include/AHO/*.hpp")
    foreach(header IN LISTS headers)
        file(RELATIVE_PATH relative "${CMAKE_CURRENT_SOURCE_DIR}/../include/AHO" "${header}")
        if(relative STREQUAL "x/core/mesh.hpp" AND NOT AHO_ENABLE_EXPERIMENTAL_MESH)
            continue()
        endif()
        string(REPLACE "/" "." name "${relative}")
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/header-contracts")
        set(source "${CMAKE_CURRENT_BINARY_DIR}/header-contracts/${name}.cpp")
        file(WRITE "${source}" "#include <AHO/${relative}>\n")
        aho_add_compile_test("Header.${name}" "${source}" "")
    endforeach()
endfunction()

function(aho_add_link_test name source definition)
    string(MAKE_C_IDENTIFIER "aho_link_${name}" target)
    add_executable(${target} EXCLUDE_FROM_ALL "${source}")
    target_compile_definitions(${target} PRIVATE "${definition}")
    target_compile_options(${target} PRIVATE "$<TARGET_PROPERTY:aho-test,COMPILE_OPTIONS>")
    target_include_directories(${target} PRIVATE "$<TARGET_PROPERTY:aho-test,INCLUDE_DIRECTORIES>")
    target_link_libraries(${target} PRIVATE AHOLi VSL GTest::gtest_main)
    target_link_directories(${target} PRIVATE ${GLFW_LIBRARY_DIRS})
    set(kind Link)
    set(run_program FALSE)
    if(ARGC GREATER 3 AND ARGV3)
        set(kind Contract)
        set(run_program TRUE)
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target} PRIVATE -ferror-limit=0)
    endif()
    add_test(NAME "AHO.${kind}.${name}" COMMAND "${CMAKE_COMMAND}"
            "-DTEST_BUILD_DIR=${CMAKE_BINARY_DIR}" "-DTEST_TARGET=${target}"
            "-DTEST_CONFIGURATION=$<CONFIG>" "-DTEST_PROGRAM=$<TARGET_FILE:${target}>"
            "-DTEST_RUN_PROGRAM=${run_program}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBuildContract.cmake")
    set_tests_properties("AHO.${kind}.${name}" PROPERTIES LABELS "AHO;contract" TIMEOUT 90 RESOURCE_LOCK aho_build_tree)
endfunction()
