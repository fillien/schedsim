enable_testing()
find_package(GTest REQUIRED)

add_executable(tests)
target_sources(tests PRIVATE
  "main.cpp"
  "scenario.cpp"
  "traces.cpp"
  "platform.cpp"
)

target_link_libraries(
  tests
  GTest::gtest
  GTest::gtest_main
  scenario
  traces
  platform
  json
)

gtest_discover_tests(tests)
