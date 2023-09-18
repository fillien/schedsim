cmake_minimum_required(VERSION 3.10.2)
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
FetchContent_MakeAvailable(yaml-cpp)

add_subdirectory(doc)
add_subdirectory(src)
