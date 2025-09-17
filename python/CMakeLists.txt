cmake_minimum_required(VERSION 3.20...3.25)
project(pyschedlib)

find_package(SWIG REQUIRED)
include(UseSWIG)

# Align dev headers with the interpreter actually found
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Python3 ${Python3_VERSION} EXACT REQUIRED COMPONENTS Development)

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

if(BUILD_TESTING)
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
endif()

# Install the generated Python file and the module
install(TARGETS pyschedlib DESTINATION .)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pyschedlib.py DESTINATION .)
