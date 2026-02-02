cmake_minimum_required(VERSION 3.20...3.25)
project(pyschedlib)

find_package(SWIG REQUIRED)
include(UseSWIG)

# Align dev headers with the interpreter actually found
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Python3 ${Python3_VERSION} EXACT REQUIRED COMPONENTS Development)

# ============================================================================
# pyschedlib - Legacy bindings for schedlib
# ============================================================================

set_source_files_properties(src/pyschedlib.i PROPERTIES CPLUSPLUS ON)

swig_add_library(pyschedlib
  TYPE MODULE
  LANGUAGE python
  SOURCES src/pyschedlib.i
)

# Ensure target include dirs are used by SWIG and the wrapper
set_property(TARGET pyschedlib PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)

target_link_libraries(pyschedlib
  PRIVATE
  Python3::Module
  schedlib
)

set_target_properties(pyschedlib PROPERTIES
  CXX_VISIBILITY_PRESET default
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Headers from the C++ library
target_include_directories(pyschedlib
  PRIVATE
  ${CMAKE_SOURCE_DIR}/schedlib/include
)

# ============================================================================
# pyschedsim - New bindings for schedsim three-library architecture
# ============================================================================

set_source_files_properties(src/pyschedsim.i PROPERTIES CPLUSPLUS ON)

# Set include directories for SWIG to find the .i files
set(CMAKE_SWIG_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/src")

swig_add_library(pyschedsim
  TYPE MODULE
  LANGUAGE python
  SOURCES src/pyschedsim.i
)

set_property(TARGET pyschedsim PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)

target_link_libraries(pyschedsim
  PRIVATE
  Python3::Module
  schedsim-core
  schedsim-algo
  schedsim-io
)

set_target_properties(pyschedsim PROPERTIES
  CXX_VISIBILITY_PRESET default
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Headers from the schedsim libraries
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
  # Legacy pyschedlib tests
  add_test(
    NAME pyschedlib_smoke
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/pyschedlib_smoke.py
  )
  # Ensure the test can import the freshly built module
  set_tests_properties(pyschedlib_smoke PROPERTIES
    ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}"
  )

  add_test(
    NAME pyschedlib_json
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/pyschedlib_json.py
  )
  set_tests_properties(pyschedlib_json PROPERTIES
    ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}"
  )

  add_test(
    NAME pyschedlib_exceptions
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/pyschedlib_exceptions.py
  )
  set_tests_properties(pyschedlib_exceptions PROPERTIES
    ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}"
  )

  add_test(
    NAME pyschedlib_hardening
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/pyschedlib_hardening.py
  )
  set_tests_properties(pyschedlib_hardening PROPERTIES
    ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}"
  )

  # New pyschedsim tests
  add_test(
    NAME pyschedsim_core
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_core.py
  )
  set_tests_properties(pyschedsim_core PROPERTIES
    ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )

  add_test(
    NAME pyschedsim_io
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_io.py
  )
  set_tests_properties(pyschedsim_io PROPERTIES
    ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )

  add_test(
    NAME pyschedsim_types
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_types.py
  )
  set_tests_properties(pyschedsim_types PROPERTIES
    ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )

  add_test(
    NAME pyschedsim_exceptions
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_exceptions.py
  )
  set_tests_properties(pyschedsim_exceptions PROPERTIES
    ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )

  add_test(
    NAME pyschedsim_energy
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_energy.py
  )
  set_tests_properties(pyschedsim_energy PROPERTIES
    ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )

  add_test(
    NAME pyschedsim_edge_cases
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_edge_cases.py
  )
  set_tests_properties(pyschedsim_edge_cases PROPERTIES
    ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
endif()

# ============================================================================
# Install
# ============================================================================

# Install the generated Python file and the module
install(TARGETS pyschedlib DESTINATION .)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pyschedlib.py DESTINATION .)

install(TARGETS pyschedsim DESTINATION .)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pyschedsim.py DESTINATION .)
