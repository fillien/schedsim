FetchContent_Declare(cxxopts
  GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
  GIT_TAG        eb78730
)
FetchContent_MakeAvailable(cxxopts)

add_executable(generator)

target_compile_features(generator PRIVATE cxx_std_23)
#target_compile_options(generator PRIVATE -fdiagnostics-color=always -fsanitize=address)
#target_link_options(generator PRIVATE -fsanitize=address)

target_sources(generator PRIVATE
  src/main.cpp
  src/task_generator.cpp
)

target_link_libraries(generator cxxopts scenario)
