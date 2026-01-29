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

add_executable(schedsim-io-tests
    trace_writers_test.cpp
    platform_loader_test.cpp
    scenario_loader_test.cpp
    scenario_injection_test.cpp
    scenario_generation_test.cpp
    metrics_test.cpp
)

target_link_libraries(schedsim-io-tests PRIVATE
    schedsim-io
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(schedsim-io-tests)
