if(NOT SCHEDSIM_BUILD_DOCS)
  return()
endif()

find_package(Doxygen REQUIRED)
find_package(Sphinx REQUIRED)

# --- Doxygen: generate XML from annotated headers ---
set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/doxygen")
set(DOXYGEN_XML_OUTPUT "${DOXYGEN_OUTPUT_DIR}/xml")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in"
  "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
  @ONLY
)

add_custom_target(doxygen
  COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Generating Doxygen XML"
  VERBATIM
)

# --- Sphinx: generate HTML from RST + Doxygen XML via Breathe ---
set(SPHINX_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/source")
set(SPHINX_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/html")

configure_file(
  "${SPHINX_SOURCE_DIR}/conf.py.in"
  "${CMAKE_CURRENT_BINARY_DIR}/conf.py"
  @ONLY
)

add_custom_target(docs
  COMMAND "${Sphinx_BUILD_EXECUTABLE}" -b html
    -c "${CMAKE_CURRENT_BINARY_DIR}"
    "${SPHINX_SOURCE_DIR}"
    "${SPHINX_OUTPUT_DIR}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  COMMENT "Building Sphinx HTML documentation"
  DEPENDS doxygen
  VERBATIM
)

# --- Install rule ---
install(
  DIRECTORY "${SPHINX_OUTPUT_DIR}/"
  DESTINATION "share/doc/schedsim"
  OPTIONAL
)
