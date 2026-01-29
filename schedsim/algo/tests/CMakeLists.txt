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
)

target_link_libraries(schedsim_algo_tests PRIVATE
    schedsim-algo
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(schedsim_algo_tests)
