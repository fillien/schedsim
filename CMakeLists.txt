cmake_minimum_required(VERSION 3.20...3.25)
project("Schedsim"
  VERSION 2023.09
  DESCRIPTION "A scheduling simulator")

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -Wno-unused -Wno-uninitialized")

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(ENABLE_TRACY OFF)
else()
    set(ENABLE_TRACY ON)
endif()

add_subdirectory(external/cxxopts)
add_subdirectory(external/json)

add_subdirectory(doc)
add_subdirectory(protocols)
add_subdirectory(schedview)
add_subdirectory(schedgen)
add_subdirectory(schedsim)
add_subdirectory(tests)

install(
  TARGETS schedsim schedview schedgen
  EXPORT ${PROJECT_NAME}_Targets
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUEDIR}
)

file(GLOB_RECURSE ALL_SOURCE_FILES "*.cpp" "*.hpp")
list(FILTER ALL_SOURCE_FILES EXCLUDE REGEX "external/")
list(FILTER ALL_SOURCE_FILES EXCLUDE REGEX "build/")
list(FILTER ALL_SOURCE_FILES EXCLUDE REGEX ".git/")

add_custom_target(
        clang-format
        COMMAND clang-format -i ${ALL_SOURCE_FILES}
)

# Clang-Tidy configuration
find_program(CLANG_TIDY_EXE NAMES "clang-tidy")

if(NOT CLANG_TIDY_EXE)
    message(WARNING "Clang-Tidy not found!")
else()
    set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}")

    add_custom_target(
        clang-tidy
        COMMAND ${CLANG_TIDY_COMMAND}
        -p ${CMAKE_BINARY_DIR}
        ${ALL_SOURCE_FILES}
        COMMENT "Running clang-tidy checks"
    )
endif()
