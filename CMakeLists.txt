cmake_minimum_required(VERSION 3.20...3.25)
project("Schedsim"
  VERSION 2023.09
  DESCRIPTION "A scheduling simulator")

include(FetchContent)
include(CTest)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -Wno-unused")

FetchContent_Declare(yaml_cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG        51adc5f7392648622c80909f65bf794fc48d013f # latest master
)

FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        6eab7a2b187b10b2494e39c1961750bfd1bda500 # latest master
)

FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        f8d7d77 # 1.14.0
)
FetchContent_MakeAvailable(yaml_cpp json googletest)

add_subdirectory(protocols)
add_subdirectory(doc)
add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(viewer)
add_subdirectory(generator)
