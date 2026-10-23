option(AHO_ENABLE_EXPERIMENTAL_MESH "Enable the saitekikuukan-backed experimental Mesh" OFF)
if(AHO_ENABLE_EXPERIMENTAL_MESH)
    include(FetchContent)
    FetchContent_Declare(saitekikuukan
        GIT_REPOSITORY https://github.com/aruzen/saitekikuukan.git
        GIT_TAG 22186d45f5cf6dab517603d4d88f2dd1ba082a00)
    FetchContent_MakeAvailable(saitekikuukan)
    add_library(aho-experimental-mesh INTERFACE)
    add_library(AHO::ExperimentalMesh ALIAS aho-experimental-mesh)
    target_compile_features(aho-experimental-mesh INTERFACE cxx_std_26)
    target_link_libraries(aho-experimental-mesh INTERFACE saitekikuukan::saitekikuukan)
    target_include_directories(aho-experimental-mesh INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/../AHO/include"
        "${VSL_HOME}/include")
    option(AHO_BUILD_MESH_EXAMPLE "Build the experimental Mesh example" OFF)
    if(AHO_BUILD_MESH_EXAMPLE)
        add_executable(aho-mesh-example "${CMAKE_CURRENT_LIST_DIR}/../AHO/examples/mesh.cpp")
        target_link_libraries(aho-mesh-example PRIVATE AHO::ExperimentalMesh)
    endif()
endif()
