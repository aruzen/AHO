option(AHO_ENABLE_EXPERIMENTAL_MESH "Enable the saitekikuukan-backed experimental Mesh" OFF)
if(AHO_ENABLE_EXPERIMENTAL_MESH)
    option(AHO_REQUIRE_MESH_REFLECTION "Require automatic C++26 Mesh attribute reflection" OFF)
    include(CheckCXXSourceCompiles)
    include(CMakePushCheckState)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_INCLUDES "${saitekikuukan_SOURCE_DIR}/include")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(AHO_MESH_REFLECTION_FLAG -freflection)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag(-freflection-latest AHO_MESH_CLANG_REFLECTION)
        if(AHO_MESH_CLANG_REFLECTION)
            set(AHO_MESH_REFLECTION_FLAG -freflection-latest)
        endif()
    endif()
    set(CMAKE_REQUIRED_FLAGS "${AHO_MESH_REFLECTION_FLAG}")
    check_cxx_source_compiles([=[
        #include <saitekikuukan.hpp>
        #if !defined(__cpp_impl_reflection) || __cpp_impl_reflection < 202506L
        #error C++26 reflection is required
        #endif
        struct Row { int value = 7; };
        int main() {
            SAITEKIKUUKAN_NAMESPACE::record_soa<Row> rows;
            rows.resize(1);
            rows.column<^^Row::value>()[0] = 42;
            return rows.read(0).value != 42;
        }
    ]=] AHO_MESH_HAS_REFLECTION)
    cmake_pop_check_state()
    if(AHO_REQUIRE_MESH_REFLECTION AND NOT AHO_MESH_HAS_REFLECTION)
        message(FATAL_ERROR "Mesh reflection requires GCC 16 with -freflection or a compatible reflection toolchain")
    endif()

    add_library(aho-experimental-mesh INTERFACE)
    add_library(AHO::ExperimentalMesh ALIAS aho-experimental-mesh)
    target_compile_features(aho-experimental-mesh INTERFACE cxx_std_26)
    if(AHO_MESH_HAS_REFLECTION)
        target_link_libraries(aho-experimental-mesh INTERFACE saitekikuukan::reflection)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(aho-experimental-mesh INTERFACE "${AHO_MESH_REFLECTION_FLAG}")
        endif()
        message(STATUS "Mesh attributes: automatic C++26 reflection")
    else()
        target_link_libraries(aho-experimental-mesh INTERFACE saitekikuukan::saitekikuukan)
        message(STATUS "Mesh attributes: explicit MeshAttributeSchema (reflection unavailable)")
    endif()
    target_include_directories(aho-experimental-mesh INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/../AHO/include"
        "${VSL_HOME}/include")
    option(AHO_BUILD_MESH_EXAMPLE "Build the experimental Mesh example" OFF)
    if(AHO_BUILD_MESH_EXAMPLE)
        add_executable(aho-mesh-example "${CMAKE_CURRENT_LIST_DIR}/../AHO/examples/mesh.cpp")
        target_link_libraries(aho-mesh-example PRIVATE AHO::ExperimentalMesh)
    endif()
endif()
