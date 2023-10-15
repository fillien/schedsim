cmake_minimum_required(VERSION 3.5...3.27)
include(FetchContent)

set(CMAKE_BUILD_TYPE Debug)

project("Schedsim"
  VERSION 2023.09
  DESCRIPTION "A scheduling simulator")

# Fetch dependancies
FetchContent_Declare(yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG        f732014 # 0.8
)
FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        bc889af # 3.11.2
)
FetchContent_MakeAvailable(yaml-cpp json)

add_subdirectory(doc)
add_subdirectory(src)
