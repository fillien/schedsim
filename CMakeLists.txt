cmake_minimum_required(VERSION 3.20...3.25)
project("Schedsim"
  VERSION 2023.09
  DESCRIPTION "A scheduling simulator")

include(CTest)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-fcolor-diagnostics)
endif()

if (CMAKE_GENERATOR MATCHES "Ninja")
    set(CMAKE_COLOR_MAKEFILE ON)
    add_compile_definitions(CLICOLOR=1)
    add_compile_definitions(CLICOLOR_FORCE=1)
endif()

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -Wno-unused -Wno-uninitialized")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

if(CMAKE_EXPORT_COMPILE_COMMANDS)
    set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
    ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(ENABLE_TRACY OFF)
else()
    set(ENABLE_TRACY ON)
endif()

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

configure_file(
    ${CMAKE_SOURCE_DIR}/external/version.h.in
    ${CMAKE_BINARY_DIR}/generated/version.h
    @ONLY
)

add_subdirectory(external/cxxopts)
add_subdirectory(external/json)
add_subdirectory(external/tracy)

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
