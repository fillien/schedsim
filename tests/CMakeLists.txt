enable_testing()

add_executable(tests)
target_sources(tests PRIVATE
  "main.cpp"
  "../src/engine.cpp"
  "../src/plateform.cpp"
  "../src/processor.cpp"
  "../src/scheduler.cpp"
#  "../src/sched_mono.cpp"
  "../src/sched_parallel.cpp"
  "../src/server.cpp"
  "../src/task.cpp"
)

target_link_libraries(
  tests
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(tests)
