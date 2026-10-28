function(aho_fetch_windows_glfw)
    include(FetchContent)
    set(BUILD_SHARED_LIBS ON)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(aho_glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG a74efa0d5628b74adc0426af4c5710e287fa7c2c)
    FetchContent_MakeAvailable(aho_glfw)
    set(GLFW_INCLUDE_DIRS "${aho_glfw_SOURCE_DIR}/include" PARENT_SCOPE)
    set(GLFW_LIBRARIES glfw PARENT_SCOPE)
    set(GLFW_LIBRARY_DIRS "" PARENT_SCOPE)
endfunction()

if(WIN32)
    set(VULKAN_SDK "$ENV{VULKAN_SDK}" CACHE PATH "Vulkan SDK root")
    if(DEFINED ENV{GLFW_ROOT} AND NOT "$ENV{GLFW_ROOT}" STREQUAL "")
        set(GLFW_ROOT "$ENV{GLFW_ROOT}" CACHE PATH "GLFW root")
    else()
        set(GLFW_ROOT "$ENV{GLFW}" CACHE PATH "GLFW root")
    endif()

    find_path(Vulkan_INCLUDE_DIR vulkan/vulkan.h HINTS "${VULKAN_SDK}/Include" REQUIRED)
    find_library(Vulkan_LIBRARY NAMES vulkan-1 HINTS "${VULKAN_SDK}/Lib" REQUIRED)
    find_package(Vulkan REQUIRED)
    get_filename_component(Vulkan_LIBRARY_DIRS "${Vulkan_LIBRARY}" DIRECTORY)

    find_path(GLFW_INCLUDE_DIRS GLFW/glfw3.h HINTS "${GLFW_ROOT}/include")
    if(GLFW_INCLUDE_DIRS)
        file(STRINGS "${GLFW_INCLUDE_DIRS}/GLFW/glfw3.h" version_line REGEX "^#define GLFW_VERSION_MINOR +[0-9]+")
        string(REGEX MATCH "[0-9]+$" glfw_minor "${version_line}")
    endif()
    if(NOT GLFW_INCLUDE_DIRS OR glfw_minor LESS 4)
        message(STATUS "Building pinned GLFW 3.4 for Windows")
        aho_fetch_windows_glfw()
    else()
        find_library(GLFW_LIBRARIES NAMES glfw3dll HINTS "${GLFW_ROOT}/lib-vc2022" "${GLFW_ROOT}/lib-vc2019" REQUIRED)
        get_filename_component(GLFW_LIBRARY_DIRS "${GLFW_LIBRARIES}" DIRECTORY)
        find_file(GLFW_RUNTIME_LIBRARY glfw3.dll HINTS "${GLFW_LIBRARY_DIRS}" REQUIRED)
        add_library(glfw SHARED IMPORTED GLOBAL)
        set_target_properties(glfw PROPERTIES
                IMPORTED_IMPLIB "${GLFW_LIBRARIES}"
                IMPORTED_LOCATION "${GLFW_RUNTIME_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${GLFW_INCLUDE_DIRS}"
                INTERFACE_COMPILE_DEFINITIONS GLFW_DLL)
    endif()

    if(NOT TARGET glm::glm)
        find_path(GLM_INCLUDE_DIR glm/glm.hpp HINTS "${Vulkan_INCLUDE_DIRS}" REQUIRED)
        add_library(glm::glm INTERFACE IMPORTED GLOBAL)
        target_include_directories(glm::glm INTERFACE "${GLM_INCLUDE_DIR}")
    endif()

    find_package(Boost COMPONENTS system QUIET)
endif()

function(aho_copy_windows_runtime)
    if(WIN32)
        get_property(targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
        foreach(target IN LISTS targets)
            get_target_property(type "${target}" TYPE)
            if(type STREQUAL "EXECUTABLE")
                add_custom_command(TARGET "${target}" POST_BUILD
                        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                                "$<TARGET_FILE:glfw>" "$<TARGET_FILE_DIR:${target}>"
                        VERBATIM)
            endif()
        endforeach()
    endif()
endfunction()
