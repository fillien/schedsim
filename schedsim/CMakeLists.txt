add_executable(schedsim)

target_compile_features(schedsim PRIVATE cxx_std_23)
target_compile_options(schedsim PRIVATE -fdiagnostics-color=always -fsanitize=address)
target_compile_options(schedsim PRIVATE -O3)
target_link_options(schedsim PRIVATE -fsanitize=address)
target_link_libraries(schedsim PRIVATE cxxopts protocols)

target_sources(schedsim PRIVATE
  src/main.cpp
  src/engine.cpp
  src/plateform.cpp
  src/processor.cpp
  src/scheduler.cpp
#  sched_mono.cpp
  src/sched_parallel.cpp
  src/server.cpp
  src/task.cpp
)
