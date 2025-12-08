# cmake/AHOLi.cmake

function(init_aholi AHOLI_TARGET)
    message(STATUS "Initializing AHOLi for target: ${AHOLI_TARGET}")

    # PCH
    set(AHOLI_PCH "${CMAKE_CURRENT_SOURCE_DIR}/pch.h")
    target_precompile_headers(${AHOLI_TARGET}
            PRIVATE
            ${AHOLI_PCH}
    )
    target_compile_definitions(${AHOLI_TARGET} PRIVATE USE_PCH)

    # Includes
    target_include_directories(${AHOLI_TARGET}
            PRIVATE
            ${Boost_INCLUDE_DIRS}
            ${AHO_HOME} # ~/repos/AHO/AHOLi
            ${VSL_HOME} # ~/repos/AHO/VSLi
            ${GLFW_INCLUDE_DIRS}
    )

    # Linking
    target_link_directories(${AHOLI_TARGET}
            PRIVATE
            lib
            ${GLFW_LIBRARY_DIRS}
    )

    target_link_libraries(${AHOLI_TARGET}
            PRIVATE
            ${Boost_LIBRARIES}
            VSL
            ${GLFW_LIBRARIES}
            glm::glm
    )

    # Warnings
    if (MSVC)
        target_compile_options(${AHOLI_TARGET} PRIVATE /W4 /WX)
    else()
        target_compile_options(${AHOLI_TARGET} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
