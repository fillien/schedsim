# Three-library architecture
add_subdirectory(core)
add_subdirectory(algo)
add_subdirectory(io)

if(SCHEDSIM_BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()
