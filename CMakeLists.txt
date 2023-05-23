cmake_minimum_required(VERSION 3.10.2)
include(FetchContent)

set(CMAKE_BUILD_TYPE Debug)

project("Schedsim"
  VERSION 0.1
  DESCRIPTION "A scheduling simulator")

# Fetch dependancies
FetchContent_Declare(yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG        0579ae3 # 0.7
)
FetchContent_MakeAvailable(yaml-cpp)

add_subdirectory(doc)
add_subdirectory(src)
