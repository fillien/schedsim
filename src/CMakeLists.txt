# Color ninja output
#add_compile_options(-fdiagnostics-color=always -fsanitize=address)
#add_compile_options(-fdiagnostics-color=always)
#add_link_options(-fsanitize=address)

file(GLOB source
  "main.cpp"
  "engine.cpp"
  "plateform.cpp"
  "processor.cpp"
  "scheduler.cpp"
#  "sched_mono.cpp"
  "sched_parallel.cpp"
  "server.cpp"
  "tracer_json.cpp"
  "task.cpp"
)

add_executable(launch ${source})
target_include_directories(launch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../protocols)
target_link_libraries(launch PRIVATE protocols yaml-cpp nlohmann_json)
