add_executable(schedsim)

target_compile_features(schedsim PRIVATE cxx_std_23)
target_compile_options(schedsim PRIVATE -fdiagnostics-color=always)
target_compile_options(schedsim PRIVATE -O3)

# Set Tracy option based on build type
if (ENABLE_TRACY)
    message(STATUS "Tracy is enabled")
    set(ENABLE_TRACY ON)
    FetchContent_Declare(tracy
	GIT_REPOSITORY https://github.com/wolfpld/tracy.git
	GIT_TAG v0.10
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(tracy)
    target_link_libraries(schedsim PRIVATE TracyClient cxxopts protocols)
else()
    message(STATUS "Tracy is disabled")
    set(ENABLE_TRACY OFF)
    target_link_libraries(schedsim PRIVATE cxxopts protocols)
endif()

target_sources(schedsim PRIVATE
    src/main.cpp
    src/engine.cpp
    src/platform.cpp
    src/processor.cpp
    src/scheduler.cpp
    src/schedulers/parallel.cpp
    src/schedulers/power_aware.cpp
    src/schedulers/power_aware_f_min.cpp
    src/schedulers/power_aware_m_min.cpp
    src/server.cpp
    src/task.cpp
)
target_include_directories(schedsim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

enable_testing()
find_package(GTest REQUIRED)

add_executable(schedsim_tests)
target_sources(schedsim_tests PRIVATE
    tests/main.cpp
    src/engine.cpp
    src/platform.cpp
    src/processor.cpp
    src/scheduler.cpp
    src/schedulers/parallel.cpp
    src/schedulers/power_aware.cpp
    src/schedulers/power_aware_f_min.cpp
    src/schedulers/power_aware_m_min.cpp
    src/server.cpp
    src/task.cpp
)

target_include_directories(schedsim_tests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(
    schedsim_tests
    protocols
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(schedsim_tests)
