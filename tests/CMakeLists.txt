enable_testing()

file(GLOB source
  "main.cpp"
  "../src/engine.cpp"
  "../src/plateform.cpp"
  "../src/processor.cpp"
  "../src/scheduler.cpp"
  "../src/sched_mono.cpp"
  "../src/sched_parallel.cpp"
  "../src/server.cpp"
  "../src/tracer_json.cpp"
  "../src/task.cpp"
)

add_executable(tests ${source})
target_link_libraries(
  tests
  GTest::gtest_main
  nlohmann_json
)

include(GoogleTest)
gtest_discover_tests(tests)
