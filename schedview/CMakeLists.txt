add_executable(schedview)

target_compile_features(schedview PRIVATE cxx_std_23)
if (NOT APPLE)
    target_compile_options(schedview PRIVATE -fdiagnostics-color=always -fsanitize=address)
    target_link_options(schedview PRIVATE -fsanitize=address)
endif()

# Set Tracy option based on build type
if (ENABLE_TRACY)
    message(STATUS "Tracy is enabled")
    set(ENABLE_TRACY ON)
    target_link_libraries(schedview PRIVATE TracyClient cxxopts protocols)
else()
    message(STATUS "Tracy is disabled")
    set(ENABLE_TRACY OFF)
    target_link_libraries(schedview PRIVATE cxxopts protocols)
endif()

target_sources(schedview PRIVATE
  src/main.cpp
  src/gantt/gantt.cpp
  src/gantt/svg.cpp
  src/gantt/rtsched.cpp
  src/textual.cpp
  src/energy.cpp
  src/energy_model.cpp
  src/frequency.cpp
  src/stats.cpp
  src/deadline_misses.cpp
  src/system_utilization.cpp
)

target_include_directories(schedview PUBLIC ${CMAKE_BINARY_DIR}/generated ${CMAKE_CURRENT_SOURCE_DIR}/include)

enable_testing()
find_package(GTest REQUIRED)

add_executable(schedview_tests)
target_sources(schedview_tests PRIVATE
  tests/main.cpp
  src/gantt/gantt.cpp
  src/gantt/svg.cpp
  src/gantt/rtsched.cpp
  src/textual.cpp
  src/energy.cpp
  src/energy_model.cpp
  src/frequency.cpp
  src/stats.cpp
  src/deadline_misses.cpp
  src/system_utilization.cpp
)

message(STATUS ${CMAKE_BINARY_DIR})
target_include_directories(schedview_tests PUBLIC ${CMAKE_BINARY_DIR}/generated ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(
    schedview_tests
    protocols
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(schedview_tests)
