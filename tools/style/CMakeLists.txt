cmake_minimum_required(VERSION 3.30)
project(AHOStyleTools LANGUAGES C CXX)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
if(NOT LLVM_VERSION_MAJOR EQUAL 20)
    message(FATAL_ERROR "Style checks require LLVM 20")
endif()

add_executable(aho-style-ast ast_check.cpp)
target_compile_features(aho-style-ast PRIVATE cxx_std_26)
target_include_directories(aho-style-ast SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
target_link_libraries(aho-style-ast PRIVATE clang-cpp LLVM)
execute_process(COMMAND "${LLVM_TOOLS_BINARY_DIR}/clang" -print-resource-dir
    OUTPUT_VARIABLE AHO_STYLE_RESOURCE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
target_compile_definitions(aho-style-ast PRIVATE "AHO_STYLE_RESOURCE_DIR=\"${AHO_STYLE_RESOURCE_DIR}\"")

find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
find_program(AHO_CLANG_FORMAT NAMES clang-format-20 clang-format HINTS "${LLVM_TOOLS_BINARY_DIR}" REQUIRED)
find_program(AHO_CLANG_TIDY NAMES clang-tidy-20 clang-tidy HINTS "${LLVM_TOOLS_BINARY_DIR}" REQUIRED)
enable_testing()
add_test(NAME Style.CheckerContracts
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_checks.py"
        --ast-tool "$<TARGET_FILE:aho-style-ast>"
        --clang-tidy "${AHO_CLANG_TIDY}" --clang-format "${AHO_CLANG_FORMAT}")

if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # 通常のTUが読まない公開umbrellaと空の予約ヘッダーも解析対象にする。
    add_library(aho-style-headers OBJECT EXCLUDE_FROM_ALL headers.cpp)
    target_link_libraries(aho-style-headers PRIVATE saitekikuukan::saitekikuukan)
    target_include_directories(aho-style-headers PRIVATE
        "${AHO_HOME}/include" "${VSL_HOME}/include"
        ${Vulkan_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS})
    include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/StyleChecks.cmake")
endif()
