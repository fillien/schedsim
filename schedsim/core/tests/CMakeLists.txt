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
)

target_link_libraries(schedsim-core-tests PRIVATE
    schedsim-core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(schedsim-core-tests)
