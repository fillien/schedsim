enable_testing()
find_package(GTest REQUIRED)

add_executable(tests)
target_sources(tests PRIVATE
  "main.cpp"
  "scenario.cpp"
  "traces.cpp"
  "hardware.cpp"
)

target_link_libraries(
  tests
  GTest::gtest
  GTest::gtest_main
  protocols
  json
)

gtest_discover_tests(tests)
