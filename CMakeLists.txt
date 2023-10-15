cmake_minimum_required(VERSION 3.5...3.27)
include(FetchContent)

project("Schedsim"
  VERSION 2023.09
  DESCRIPTION "A scheduling simulator")

FetchContent_Declare(yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG        f732014 # 0.8
)
FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        bc889af # 3.11.2
)
FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        f8d7d77 # 1.14.0
)

FetchContent_MakeAvailable(yaml-cpp json googletest)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -Wno-unused")

add_subdirectory(doc)
add_subdirectory(src)
add_subdirectory(tests)
