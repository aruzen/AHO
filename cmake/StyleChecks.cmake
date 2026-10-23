set(AHO_STYLE_JOBS 2 CACHE STRING "Parallel translation units for style checks")
set(AHO_STYLE_IN_CLASS_MIN_LINES 5 CACHE STRING "Minimum body code lines requiring an out-of-class definition")
set(AHO_STYLE_MASTER_DIRECTORY "${CMAKE_SOURCE_DIR}/tools/style/master" CACHE PATH "Per-source style master directory")
add_custom_target(check-format
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/style/check.py"
        --checks=format --master "${AHO_STYLE_MASTER_DIRECTORY}"
        --in-class-min-lines "${AHO_STYLE_IN_CLASS_MIN_LINES}"
        --clang-format "${AHO_CLANG_FORMAT}" --jobs "${AHO_STYLE_JOBS}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    USES_TERMINAL VERBATIM)

add_custom_target(check-style
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/style/check.py"
        --database "${CMAKE_BINARY_DIR}" --ast-tool "$<TARGET_FILE:aho-style-ast>"
        --master "${AHO_STYLE_MASTER_DIRECTORY}"
        --in-class-min-lines "${AHO_STYLE_IN_CLASS_MIN_LINES}"
        --clang-format "${AHO_CLANG_FORMAT}" --clang-tidy "${AHO_CLANG_TIDY}" --jobs "${AHO_STYLE_JOBS}"
    DEPENDS aho-style-ast
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    USES_TERMINAL VERBATIM)

add_custom_target(update-style-master
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/style/check.py"
        --database "${CMAKE_BINARY_DIR}" --ast-tool "$<TARGET_FILE:aho-style-ast>"
        --master "${AHO_STYLE_MASTER_DIRECTORY}" --update-master
        --in-class-min-lines "${AHO_STYLE_IN_CLASS_MIN_LINES}"
        --clang-format "${AHO_CLANG_FORMAT}" --clang-tidy "${AHO_CLANG_TIDY}" --jobs "${AHO_STYLE_JOBS}"
    DEPENDS aho-style-ast
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    USES_TERMINAL VERBATIM)
