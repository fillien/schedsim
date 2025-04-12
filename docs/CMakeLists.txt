# Configure documentation generator
find_package(Doxygen REQUIRED dot)
find_package(Sphinx REQUIRED)

if(DOXYGEN_FOUND)
    set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    set(DOXYGEN_EXCLUDE_PATTERNS build */external/*)
    set(DOXYGEN_BUILTIN_STL_SUPPORT YES)
    set(DOXYGEN_CPP_CLI_SUPPORT YES)
    set(DOXYGEN_EXTRACT_ALL YES)
    set(DOXYGEN_EXTRACT_PRIVATE YES)
    set(DOXYGEN_HAVE_DOT YES)
    set(DOXYGEN_TRANSPARENT YES)
    set(DOXYGEN_TAB_SIZE 8)
    set(DOXYGEN_GENERATE_TODOLIST YES)
    set(DOXYGEN_GENERATE_XML YES)

    doxygen_add_docs(
            doxygen
            ${PROJECT_SOURCE_DIR}
            COMMENT "Generate XML documentation"
        )

    if (Sphinx_FOUND)
        set(SPHINX_SOURCE_DIR "${CMAKE_SOURCE_DIR}/docs/source")  # Path to your Sphinx source (e.g., where conf.py is)
        set(SPHINX_OUTPUT_DIR "${CMAKE_BINARY_DIR}/docs")  # Where to output the built docs

        add_custom_target(docs
            COMMAND ${Sphinx_BUILD_EXECUTABLE} -b html
                    ${SPHINX_SOURCE_DIR} ${SPHINX_OUTPUT_DIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating documentation with Sphinx"
            VERBATIM
        )
    endif()
else(DOXYGEN_FOUND)
    message("Doxygen need to be installed to generate the doxygen documentation")
endif(DOXYGEN_FOUND)
