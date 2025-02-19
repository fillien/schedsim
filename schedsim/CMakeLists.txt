add_executable(schedsim)

target_compile_features(schedsim PRIVATE cxx_std_23)
target_compile_options(schedsim PRIVATE -O3)
target_compile_options(schedsim PRIVATE -fsanitize=address)
target_link_options(schedsim PRIVATE -fsanitize=address)
if(NOT APPLE)
    target_compile_options(schedsim PRIVATE -fdiagnostics-color=always -fsanitize=address)
    target_link_options(schedsim PRIVATE -fsanitize=address)
endif()

# Set Tracy option based on build type
if (ENABLE_TRACY)
    message(STATUS "Tracy is enabled")
    set(ENABLE_TRACY ON)
    target_compile_options(TracyClient PRIVATE -Wno-unused-parameter -Wno-missing-field-initializers -Wno-c11-extensions)
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
    src/allocator.cpp
    src/allocators/low_perf_first.cpp
    src/allocators/high_perf_first.cpp
    src/allocators/smart_ass.cpp
    src/scheduler.cpp
    src/schedulers/parallel.cpp
    src/schedulers/power_aware.cpp
    src/schedulers/power_aware_timer.cpp
    src/schedulers/dpm_dvfs.cpp
    src/schedulers/ffa.cpp
    src/schedulers/ffa_timer.cpp
    src/schedulers/csf.cpp
    src/schedulers/csf_timer.cpp
    src/server.cpp
    src/task.cpp
    src/timer.cpp
)
target_include_directories(schedsim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/generated)

# enable_testing()
# find_package(GTest REQUIRED)

# add_executable(schedsim_tests)
# target_sources(schedsim_tests PRIVATE
#     tests/main.cpp
#     src/engine.cpp
#     src/platform.cpp
#     src/processor.cpp
#     src/meta_scheduler.cpp
#     src/scheduler.cpp
#     src/schedulers/parallel.cpp
#     src/schedulers/power_aware.cpp
#     src/schedulers/power_aware_timer.cpp
#     src/schedulers/ffa.cpp
#     src/schedulers/ffa_timer.cpp
#     src/schedulers/csf.cpp
#     src/schedulers/csf_timer.cpp
#     src/server.cpp
#     src/task.cpp
#     src/timer.cpp
# )

# target_include_directories(schedsim_tests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# target_link_libraries(
#     schedsim_tests
#     protocols
#     GTest::gtest
#     GTest::gtest_main
# )

# gtest_discover_tests(schedsim_tests)
