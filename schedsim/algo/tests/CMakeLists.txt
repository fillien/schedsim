# Algorithm library tests

find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(schedsim_algo_tests
    cbs_server_test.cpp
    edf_scheduler_test.cpp
    edf_integration_test.cpp
    allocator_test.cpp
    # Phase 6: Policy tests
    grub_policy_test.cpp
    cash_policy_test.cpp
    dvfs_policy_test.cpp
    dpm_policy_test.cpp
    policy_integration_test.cpp
    # FFA/CSF policy tests
    ffa_policy_test.cpp
    csf_policy_test.cpp
    # FFA/CSF integration tests
    ffa_csf_integration_test.cpp
    # DVFS energy integration tests
    dvfs_integration_test.cpp
    # Cluster, multi-cluster allocator, and timer policy tests
    cluster_test.cpp
    multi_cluster_allocator_test.cpp
    ffa_timer_policy_test.cpp
    csf_timer_policy_test.cpp
    allocators_test.cpp
    dvfs_dpm_utils_test.cpp
)

target_link_libraries(schedsim_algo_tests PRIVATE
    schedsim-algo
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(schedsim_algo_tests)
