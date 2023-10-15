set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -Wno-unused")

# Color ninja output
add_compile_options(-fdiagnostics-color=always -fsanitize=address)
#add_compile_options(-fdiagnostics-color=always)
add_link_options(-fsanitize=address)

file(GLOB source
  "main.cpp"
  "engine.cpp"
  "plateform.cpp"
  "processor.cpp"
  "scheduler.cpp"
  "sched_mono.cpp"
  "sched_parallel.cpp"
  "server.cpp"
  "tracer_json.cpp"
  "task.cpp"
)

add_executable(launch ${source})
target_link_libraries(launch PRIVATE yaml-cpp nlohmann_json)
