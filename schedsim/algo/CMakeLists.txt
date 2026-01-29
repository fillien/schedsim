add_library(schedsim-algo STATIC)

target_compile_features(schedsim-algo PUBLIC cxx_std_20)

target_link_libraries(schedsim-algo PUBLIC schedsim-core)

target_sources(schedsim-algo PRIVATE
    src/algo.cpp
    src/cbs_server.cpp
    src/edf_scheduler.cpp
    src/single_scheduler_allocator.cpp
    # Phase 6: Reclamation and Power Policies
    src/reclamation_policy.cpp
    src/grub_policy.cpp
    src/cash_policy.cpp
    src/dvfs_policy.cpp
    src/dpm_policy.cpp
)

target_include_directories(schedsim-algo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(SCHEDSIM_BUILD_TESTS)
    add_subdirectory(tests)
endif()
