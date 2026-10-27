if(AHO_ENABLE_MSDF)
    include(FetchContent)
    set(MSDFGEN_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
    set(MSDFGEN_USE_VCPKG OFF CACHE BOOL "" FORCE)
    set(MSDFGEN_USE_SKIA OFF CACHE BOOL "" FORCE)
    set(MSDFGEN_DISABLE_SVG ON CACHE BOOL "" FORCE)
    set(MSDFGEN_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(MSDFGEN_CORE_ONLY OFF CACHE BOOL "" FORCE)
    set(MSDFGEN_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_Declare(msdfgen
        GIT_REPOSITORY https://github.com/Chlumsky/msdfgen.git
        GIT_TAG 1c106ed8117893bf943e577f62eb0665fb271e46)
    FetchContent_MakeAvailable(msdfgen)
    add_library(aho-msdf STATIC
        "${CMAKE_CURRENT_LIST_DIR}/../AHO/src/x/resource/msdf_font.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../AHO/src/x/text/text_layout.cpp")
    add_library(AHO::MSDF ALIAS aho-msdf)
    set_target_properties(aho-msdf PROPERTIES EXPORT_NAME MSDF)
    target_compile_features(aho-msdf PUBLIC cxx_std_26)
    target_include_directories(aho-msdf PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../AHO/include>"
        "$<BUILD_INTERFACE:${VSL_HOME}/include>"
        "$<INSTALL_INTERFACE:include>")
    target_link_libraries(aho-msdf PUBLIC AHOLi PRIVATE msdfgen::msdfgen)
    install(TARGETS aho-msdf EXPORT AHOTargets ARCHIVE DESTINATION lib)
    option(AHO_BUILD_MSDF_EXAMPLE "Build the headless MSDF resource example" OFF)
    if(AHO_BUILD_MSDF_EXAMPLE)
        add_executable(aho-msdf-example "${CMAKE_CURRENT_LIST_DIR}/../AHO/examples/msdf.cpp")
        target_link_directories(aho-msdf-example PRIVATE ${GLFW_LIBRARY_DIRS})
        target_link_libraries(aho-msdf-example PRIVATE AHO::MSDF)
    endif()
    if(BUILD_TESTING)
        add_executable(aho-msdf-test
            "${CMAKE_CURRENT_LIST_DIR}/../AHO/test/x/resource/msdf_font_test.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/../AHO/test/x/text/text_layout_test.cpp")
        target_link_libraries(aho-msdf-test PRIVATE AHO::MSDF GTest::gtest_main)
        target_link_directories(aho-msdf-test PRIVATE ${GLFW_LIBRARY_DIRS})
        target_compile_definitions(aho-msdf-test PRIVATE
            AHO_MSDF_TEST_FONT="${CMAKE_CURRENT_LIST_DIR}/../AHO/test/fixtures/msdf/TestFont.ttf")
        add_dependencies(aho-tests aho-msdf-test)
        gtest_discover_tests(aho-msdf-test TEST_PREFIX "AHO.MSDF." PROPERTIES LABELS cpu TIMEOUT 30)
    endif()
endif()
