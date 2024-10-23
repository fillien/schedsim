add_executable(schedgen)

target_compile_features(schedgen PRIVATE cxx_std_23)
target_compile_options(schedgen PRIVATE -fdiagnostics-color=always -fsanitize=address)
target_link_options(schedgen PRIVATE -fsanitize=address)

target_sources(schedgen PRIVATE
  src/main.cpp
  src/task_generator.cpp
)

target_include_directories(schedgen PUBLIC ${CMAKE_BINARY_DIR}/generated)

target_link_libraries(schedgen cxxopts protocols)
