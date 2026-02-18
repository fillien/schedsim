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

add_executable(schedsim-core-tests
    types_test.cpp
    event_key_test.cpp
    engine_basic_test.cpp
    timer_test.cpp
    deferred_test.cpp
    trace_writer_test.cpp
    processor_type_test.cpp
    clock_domain_test.cpp
    clock_domain_modes_test.cpp
    power_domain_test.cpp
    processor_test.cpp
    task_test.cpp
    job_test.cpp
    platform_test.cpp
    context_switch_test.cpp
    dvfs_transition_test.cpp
    cstate_wakeup_test.cpp
    energy_tracker_test.cpp
    engine_stop_test.cpp
)

target_link_libraries(schedsim-core-tests PRIVATE
    schedsim-core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(schedsim-core-tests)
