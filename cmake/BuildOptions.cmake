include_guard(GLOBAL)

option(AHO_NATIVE_OPTIMIZATION "Optimize AHO/VSL for the build machine CPU" OFF)

function(aho_target_build_options target)
    if(WIN32)
        target_compile_definitions(${target} PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN GLFW_DLL)
        if(MSVC)
            target_compile_options(${target} PUBLIC /permissive- /Zc:preprocessor /utf-8)
        endif()
    endif()
    target_compile_definitions(${target} PRIVATE "$<$<CONFIG:Debug>:DEBUG>")
    target_compile_options(${target} PRIVATE
        "$<$<COMPILE_LANG_AND_ID:CXX,GNU>:-fconcepts-diagnostics-depth=7>"
        "$<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:-Wno-user-defined-literals>"
        "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANG_AND_ID:CXX,GNU>>:-g2;-gdwarf-4;-fvar-tracking-assignments;-fno-omit-frame-pointer>"
        "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>>:-g3;-fstandalone-debug;-fno-omit-frame-pointer>")
    if(AHO_NATIVE_OPTIMIZATION)
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            message(FATAL_ERROR "AHO_NATIVE_OPTIMIZATION requires GCC or Clang")
        endif()
        target_compile_options(${target} PRIVATE
            "$<$<AND:$<CONFIG:Release,RelWithDebInfo>,$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>>:-march=native>")
    endif()
endfunction()
