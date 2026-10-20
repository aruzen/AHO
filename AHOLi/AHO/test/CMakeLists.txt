add_custom_target(aho-tests)

option(AHO_TEST_SANITIZERS "Instrument AHO and its tests with AddressSanitizer and UBSan" OFF)
option(AHO_TEST_COVERAGE "Collect Clang source coverage for AHO and its tests" OFF)

file(GLOB_RECURSE AHO_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(FILTER AHO_TEST_SOURCES EXCLUDE REGEX "/(io|support)/")
file(GLOB_RECURSE AHO_INPUT_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/io/*.cpp")
if (AHO_TEST_SOURCES)
    add_executable(aho-test ${AHO_TEST_SOURCES})
    target_include_directories(aho-test PRIVATE
            ${Vulkan_INCLUDE_DIRS}
            "${CMAKE_CURRENT_SOURCE_DIR}/../include"
            "$<TARGET_PROPERTY:AHOLi,INCLUDE_DIRECTORIES>"
            "$<TARGET_PROPERTY:VSL,INCLUDE_DIRECTORIES>"
    )
    target_link_libraries(aho-test PRIVATE AHOLi VSL GTest::gtest_main)
    target_link_directories(aho-test PRIVATE ${GLFW_LIBRARY_DIRS})
    add_dependencies(aho-tests aho-test)
    set(AHO_TEST_TMPDIR "${CMAKE_CURRENT_BINARY_DIR}/temporary-files")
    add_test(NAME AHO.Temp.Setup COMMAND "${CMAKE_COMMAND}" -E make_directory "${AHO_TEST_TMPDIR}")
    add_test(NAME AHO.Temp.Cleanup COMMAND "${CMAKE_COMMAND}" -E remove_directory "${AHO_TEST_TMPDIR}")
    set_tests_properties(AHO.Temp.Setup PROPERTIES FIXTURES_SETUP aho_temp_workspace LABELS infrastructure)
    set_tests_properties(AHO.Temp.Cleanup PROPERTIES FIXTURES_CLEANUP aho_temp_workspace LABELS infrastructure)
    gtest_discover_tests(aho-test TEST_PREFIX "AHO." DISCOVERY_MODE PRE_TEST
            PROPERTIES LABELS cpu TIMEOUT 20 FIXTURES_REQUIRED aho_temp_workspace
            ENVIRONMENT "AHO_TEST_TMPDIR=${AHO_TEST_TMPDIR}")
    add_executable(aho-input-test ${AHO_INPUT_TEST_SOURCES} support/input_backend.cpp)
    target_include_directories(aho-input-test PRIVATE "$<TARGET_PROPERTY:aho-test,INCLUDE_DIRECTORIES>")
    target_link_libraries(aho-input-test PRIVATE AHOLi VSL GTest::gtest_main)
    target_link_directories(aho-input-test PRIVATE ${GLFW_LIBRARY_DIRS})
    add_dependencies(aho-tests aho-input-test)
    gtest_discover_tests(aho-input-test TEST_PREFIX "AHO." DISCOVERY_MODE PRE_TEST PROPERTIES LABELS cpu TIMEOUT 20)
    add_executable(aho-gpu-test
            engine/standard_engine_test.cpp
            window_test.cpp
            pipeline/layouts_test.cpp
            drawer/draw_phase_test.cpp
            image_readback_test.cpp
            x/resource/buffer_test.cpp
            x/resource/texture_test.cpp
            x/screenshot_test.cpp
    )
    target_compile_definitions(aho-gpu-test PRIVATE AHO_GPU_TESTS=1
            VSL_TEST_SHADER_DIR="${CMAKE_BINARY_DIR}/VSLi/VSL/test/shaders")
    target_include_directories(aho-gpu-test PRIVATE "${VSL_HOME}/test")
    add_dependencies(aho-gpu-test vsl-test-shaders)
    target_include_directories(aho-gpu-test PRIVATE "$<TARGET_PROPERTY:aho-test,INCLUDE_DIRECTORIES>")
    target_link_libraries(aho-gpu-test PRIVATE AHOLi VSL GTest::gtest_main)
    target_link_directories(aho-gpu-test PRIVATE ${GLFW_LIBRARY_DIRS})
    if(APPLE)
        target_link_libraries(aho-gpu-test PRIVATE "-framework CoreGraphics")
    endif()
    add_dependencies(aho-tests aho-gpu-test)
    gtest_discover_tests(aho-gpu-test TEST_PREFIX "AHO." DISCOVERY_MODE PRE_TEST
            PROPERTIES LABELS gpu RESOURCE_LOCK aho_graphics TIMEOUT 45)
    include(cmake/CompileContracts.cmake)
    aho_register_header_tests()
    aho_add_compile_test(ImageReadback.PublicVslOnly
            "${CMAKE_CURRENT_SOURCE_DIR}/contracts/image_readback_public_vsl.cpp"
            AHO_CONTRACT_READBACK_PUBLIC_VSL)
    aho_add_compile_test(Screenshot.PublicVslOnly
            "${CMAKE_CURRENT_SOURCE_DIR}/contracts/screenshot_public_vsl.cpp"
            AHO_CONTRACT_SCREENSHOT_PUBLIC_VSL)
    aho_add_compile_test(ResourceUpload.PublicVslOnly
            "${CMAKE_CURRENT_SOURCE_DIR}/contracts/resource_upload_public_vsl.cpp"
            AHO_CONTRACT_RESOURCE_UPLOAD_PUBLIC_VSL)
    aho_add_compile_test(Mat.InitializerConstructor "${CMAKE_CURRENT_SOURCE_DIR}/core/math/mat_test.cpp" AHO_CONTRACT_MAT_INITIALIZER)
    aho_add_compile_test(Mat.RectangularFactory "${CMAKE_CURRENT_SOURCE_DIR}/core/math/mat_test.cpp" AHO_CONTRACT_MAT_RECTANGULAR_FACTORY)
    aho_add_compile_test(Mat.AxisRotation "${CMAKE_CURRENT_SOURCE_DIR}/core/math/mat_test.cpp" AHO_CONTRACT_MAT_AXIS_ROTATION)
    aho_add_link_test(Mat.View "${CMAKE_CURRENT_SOURCE_DIR}/core/math/mat_test.cpp" AHO_CONTRACT_MAT_VIEW TRUE)
    aho_add_compile_test(Mat.AxisDegreeOverload "${CMAKE_CURRENT_SOURCE_DIR}/core/math/mat_test.cpp" AHO_CONTRACT_MAT_AXIS_DEGREE_OVERLOAD)
    aho_add_compile_test(Mat.Perspective "${CMAKE_CURRENT_SOURCE_DIR}/core/math/mat_test.cpp" AHO_CONTRACT_MAT_PERSPECTIVE)
    foreach(operation RIGHT_MULTIPLY LEFT_MULTIPLY RIGHT_DIVIDE LEFT_DIVIDE)
        aho_add_compile_test("Mat.Scalar.${operation}" "${CMAKE_CURRENT_SOURCE_DIR}/core/math/mat_test.cpp" "AHO_CONTRACT_MAT_SCALAR_${operation}")
    endforeach()
    foreach(operation MIXED_ASSIGNMENT FLOAT_NORMALIZE D1_PRODUCTS CUSTOM_COORDINATES CONSTEXPR_ARITHMETIC)
        aho_add_compile_test("Vector.${operation}" "${CMAKE_CURRENT_SOURCE_DIR}/core/vector_test.cpp" "AHO_CONTRACT_VECTOR_${operation}")
    endforeach()
    aho_add_link_test(Vector.Comparison "${CMAKE_CURRENT_SOURCE_DIR}/core/vector_test.cpp" AHO_CONTRACT_VECTOR_COMPARISON TRUE)
    aho_add_link_test(Point.Cast3D "${CMAKE_CURRENT_SOURCE_DIR}/core/point_test.cpp" AHO_CONTRACT_POINT_CAST_3D TRUE)
    aho_add_link_test(Point.Comparison "${CMAKE_CURRENT_SOURCE_DIR}/core/point_test.cpp" AHO_CONTRACT_POINT_COMPARISON TRUE)
    aho_add_compile_test(Point.ConstexprArithmetic "${CMAKE_CURRENT_SOURCE_DIR}/core/point_test.cpp" AHO_CONTRACT_POINT_CONSTEXPR_ARITHMETIC)
    aho_add_compile_test(Point.DefaultCoordinates "${CMAKE_CURRENT_SOURCE_DIR}/core/point_test.cpp" AHO_CONTRACT_POINT_DEFAULT_COORDINATES)
    foreach(operation RGB_ARITHMETIC RGBA_ARITHMETIC HSV_ARITHMETIC HSVA_ARITHMETIC ALPHA_CONVERSIONS)
        aho_add_link_test("Color.${operation}" "${CMAKE_CURRENT_SOURCE_DIR}/core/color_test.cpp" "AHO_CONTRACT_COLOR_${operation}" TRUE)
    endforeach()
    aho_add_compile_test(Input.MouseIntegerConstructor "${CMAKE_CURRENT_SOURCE_DIR}/io/input_test.cpp" AHO_CONTRACT_INPUT_MOUSE_INTEGER)
    foreach(operation ENABLE_AXES YZ_ADD XZ_SUBTRACT)
        aho_add_compile_test("Coordinate.${operation}" "${CMAKE_CURRENT_SOURCE_DIR}/core/math/coordinate_test.cpp" "AHO_CONTRACT_COORDINATE_${operation}")
    endforeach()
    aho_add_link_test(Coordinate.Comparison "${CMAKE_CURRENT_SOURCE_DIR}/core/math/coordinate_test.cpp" AHO_CONTRACT_COORDINATE_COMPARISON TRUE)
    aho_add_compile_test(Line.PointerPoints "${CMAKE_CURRENT_SOURCE_DIR}/core/line_test.cpp" AHO_CONTRACT_LINE_POINTER_POINTS)
    aho_add_compile_test(PtrLine.PointerPoints "${CMAKE_CURRENT_SOURCE_DIR}/core/line_test.cpp" AHO_CONTRACT_PTRLINE_POINTER_POINTS)
    aho_add_compile_test(Line.PolygonAddition "${CMAKE_CURRENT_SOURCE_DIR}/core/line_test.cpp" AHO_CONTRACT_LINE_POLYGON_ADDITION)
    aho_add_compile_test(PtrLine.PolygonAddition "${CMAKE_CURRENT_SOURCE_DIR}/core/line_test.cpp" AHO_CONTRACT_PTRLINE_POLYGON_ADDITION)
    foreach(operation TRIANGLES POINTER_POINTS POINTER_LINES POINTER_TRIANGLES POLYGON_ADDITION)
        aho_add_compile_test("Triangle.${operation}" "${CMAKE_CURRENT_SOURCE_DIR}/core/triangle_test.cpp" "AHO_CONTRACT_TRIANGLE_${operation}")
    endforeach()
    foreach(operation LINES TRIANGLES POINTER_POINTS POINTER_LINES POLYGON_ADDITION)
        aho_add_compile_test("PtrTriangle.${operation}" "${CMAKE_CURRENT_SOURCE_DIR}/core/triangle_test.cpp" "AHO_CONTRACT_PTRTRIANGLE_${operation}")
    endforeach()
    foreach(operation LINES_VIEW TRIANGLES_VIEW)
        aho_add_link_test("Polygon.${operation}" "${CMAKE_CURRENT_SOURCE_DIR}/utils/polygon_test.cpp" "AHO_CONTRACT_POLYGON_${operation}" TRUE)
    endforeach()
    aho_add_compile_test(Drawer.StandardTuple "${CMAKE_CURRENT_SOURCE_DIR}/drawer/standard_drawer_test.cpp" AHO_CONTRACT_STANDARD_DRAWER_TUPLE)
    aho_add_link_test(Drawer.Command "${CMAKE_CURRENT_SOURCE_DIR}/drawer_test.cpp" AHO_LINK_DRAWER_COMMAND TRUE)
    aho_add_link_test(Drawer.Delegate "${CMAKE_CURRENT_SOURCE_DIR}/drawer_test.cpp" AHO_LINK_DRAWER_DELEGATE TRUE)
    aho_add_link_test(Drawer.Standard "${CMAKE_CURRENT_SOURCE_DIR}/drawer/standard_drawer_test.cpp" AHO_LINK_STANDARD_DRAWER)
    aho_add_link_test(Input.KeyName "${CMAKE_CURRENT_SOURCE_DIR}/io/input_test.cpp" AHO_LINK_INPUT_KEY_NAME)
    aho_add_link_test(Input.MouseName "${CMAKE_CURRENT_SOURCE_DIR}/io/input_test.cpp" AHO_LINK_INPUT_MOUSE_NAME)

    if(AHO_TEST_SANITIZERS)
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            message(FATAL_ERROR "AHO_TEST_SANITIZERS requires a Clang or GCC toolchain")
        endif()
        foreach(target AHOLi aho-test aho-input-test aho-gpu-test)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        endforeach()
        target_link_options(AHOLi INTERFACE -fsanitize=address,undefined)
    endif()
    if(AHO_TEST_COVERAGE)
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            message(FATAL_ERROR "AHO_TEST_COVERAGE requires a Clang toolchain")
        endif()
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/coverage-profiles")
        foreach(target AHOLi aho-test aho-input-test aho-gpu-test)
            target_compile_options(${target} PRIVATE "-fprofile-instr-generate=${CMAKE_BINARY_DIR}/coverage-profiles/aho-%p-%m.profraw" -fcoverage-mapping)
        endforeach()
        target_link_options(AHOLi INTERFACE -fprofile-instr-generate)
    endif()
    add_custom_target(check-aho
            COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure -C "$<CONFIG>" -R "^AHO\\."
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            DEPENDS aho-tests
            USES_TERMINAL
            VERBATIM
    )
endif ()
