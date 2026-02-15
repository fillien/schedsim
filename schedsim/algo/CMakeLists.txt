add_library(schedsim-algo STATIC)

target_compile_features(schedsim-algo PUBLIC cxx_std_20)

target_link_libraries(schedsim-algo PUBLIC schedsim-core)

target_sources(schedsim-algo PRIVATE
    src/algo.cpp
    src/cbs_server.cpp
    src/edf_scheduler.cpp
    src/single_scheduler_allocator.cpp
    src/reclamation_policy.cpp
    src/grub_policy.cpp
    src/cash_policy.cpp
    src/dvfs_policy.cpp
    src/dpm_policy.cpp
    src/dvfs_dpm_utils.cpp
    src/ffa_policy.cpp
    src/csf_policy.cpp
    src/cluster.cpp
    src/multi_cluster_allocator.cpp
    src/ffa_timer_policy.cpp
    src/csf_timer_policy.cpp
    src/ff_big_first_allocator.cpp
    src/ff_little_first_allocator.cpp
    src/counting_allocator.cpp
    src/ff_cap_allocator.cpp
    src/ff_cap_adaptive_linear_allocator.cpp
    src/ff_cap_adaptive_poly_allocator.cpp
    src/ff_lb_allocator.cpp
    src/mcts_allocator.cpp
)

target_include_directories(schedsim-algo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(SCHEDSIM_BUILD_TESTS)
    add_subdirectory(tests)
endif()
