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

add_subdirectory(external/cxxopts)
add_subdirectory(external/json)

set(MATPLOTPP_BUILD_TESTS OFF)
set(MATPLOTPP_BUILD_EXAMPLES OFF)

add_subdirectory(scenario)
add_subdirectory(traces)
add_subdirectory(doc)
add_subdirectory(viewer)
add_subdirectory(generator)
add_subdirectory(src)

install(
  TARGETS launch viewer generator
  EXPORT ${PROJECT_NAME}_Targets
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUEDIR}
)
