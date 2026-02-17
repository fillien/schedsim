cmake_minimum_required(VERSION 3.20...3.30)
project(pyschedsim_bindings)

find_package(Python REQUIRED COMPONENTS Interpreter Development.Module Development.SABIModule)

# Try to find nanobind via CMake config first, then FetchContent fallback
find_package(nanobind CONFIG QUIET)
if(NOT nanobind_FOUND)
  message(STATUS "nanobind not found via find_package, using FetchContent")
  include(FetchContent)
  FetchContent_Declare(
    nanobind
    GIT_REPOSITORY https://github.com/wjakob/nanobind.git
    GIT_TAG        v2.4.0
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(nanobind)
endif()

# ============================================================================
# pyschedsim - nanobind module
# ============================================================================

nanobind_add_module(pyschedsim STABLE_ABI src/bindings.cpp)

target_link_libraries(pyschedsim
  PRIVATE
  schedsim-core
  schedsim-algo
  schedsim-io
)

target_include_directories(pyschedsim
  PRIVATE
  ${CMAKE_SOURCE_DIR}/schedsim/core/include
  ${CMAKE_SOURCE_DIR}/schedsim/algo/include
  ${CMAKE_SOURCE_DIR}/schedsim/io/include
)

# ============================================================================
# Tests
# ============================================================================

if(BUILD_TESTING)
  set(PYSCHEDSIM_TESTS
    test_core
    test_io
    test_types
    test_exceptions
    test_energy
    test_edge_cases
    test_new_api
  )

  foreach(test_name IN LISTS PYSCHEDSIM_TESTS)
    add_test(
      NAME pyschedsim_${test_name}
      COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/${test_name}.py
    )
    set_tests_properties(pyschedsim_${test_name} PROPERTIES
      ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}"
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
  endforeach()
endif()

# ============================================================================
# Install
# ============================================================================

install(TARGETS pyschedsim DESTINATION .)
