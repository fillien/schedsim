add_executable(launch)

target_compile_features(launch PRIVATE cxx_std_23)
#target_compile_options(launch PRIVATE -fdiagnostics-color=always -fsanitize=address)
#target_link_options(launch PRIVATE -fsanitize=address)
target_link_libraries(launch PRIVATE scenario traces)

target_sources(launch PRIVATE
  "main.cpp"
  "engine.cpp"
  "plateform.cpp"
  "processor.cpp"
  "scheduler.cpp"
#  "sched_mono.cpp"
  "sched_parallel.cpp"
  "server.cpp"
  "task.cpp"
)
