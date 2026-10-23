set(AHO_STYLE_JOBS 2 CACHE STRING "Parallel translation units for style checks")
set(AHO_STYLE_IN_CLASS_MIN_LINES 5 CACHE STRING "Minimum body code lines requiring an out-of-class definition")
add_custom_target(check-format
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/style/check.py"
        --checks=format --clang-format "${AHO_CLANG_FORMAT}" --jobs "${AHO_STYLE_JOBS}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    USES_TERMINAL VERBATIM)

add_custom_target(check-style
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/style/check.py"
        --database "${CMAKE_BINARY_DIR}" --ast-tool "$<TARGET_FILE:aho-style-ast>"
        --in-class-min-lines "${AHO_STYLE_IN_CLASS_MIN_LINES}"
        --clang-format "${AHO_CLANG_FORMAT}" --clang-tidy "${AHO_CLANG_TIDY}" --jobs "${AHO_STYLE_JOBS}"
    DEPENDS aho-style-ast
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    USES_TERMINAL VERBATIM)
