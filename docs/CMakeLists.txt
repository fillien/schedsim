include(FetchContent)

# Configure documentation generator
find_package(Doxygen
  REQUIRED dot)

if(DOXYGEN_FOUND)
    set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    set(DOXYGEN_EXCLUDE_PATTERNS build */external/*)
    # set(DOXYGEN_USE_MDFILE_AS_MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/../README.md)
    # set(DOXYGEN_GENERATE_TREEVIEW YES)
    # set(DOXYGEN_DISABLE_INDEX NO)
    # set(DOXYGEN_FULL_SIDEBAR NO)
    # set(DOXYGEN_HTML_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/header.html)
    # set(DOXYGEN_HTML_EXTRA_STYLESHEET
    #   ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/doxygen-awesome.css
    #   ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/doxygen-awesome-sidebar-only.css
    #   ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/doxygen-awesome-sidebar-only-darkmode-toggle.css
    # )
    # set(DOXYGEN_HTML_EXTRA_FILES ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/doxygen-awesome-darkmode-toggle.js)
    # set(DOXYGEN_HTML_COLORSTYLE LIGHT)
    set(DOXYGEN_BUILTIN_STL_SUPPORT YES)
    set(DOXYGEN_CPP_CLI_SUPPORT YES)
    set(DOXYGEN_EXTRACT_ALL YES)
    set(DOXYGEN_EXTRACT_PRIVATE YES)
    # set(DOXYGEN_UML_LOOK YES)
    # set(DOXYGEN_DOT_UML_DETAILS YES)
    # set(DOXYGEN_HAVE_DOT YES)
    # set(DOXYGEN_DOT_IMAGE_FORMAT svg)
    set(DOXYGEN_TRANSPARENT YES)
    set(DOXYGEN_TAB_SIZE 8)
    set(DOXYGEN_GENERATE_TODOLIST YES)
    set(DOXYGEN_GENERATE_XML YES)

    doxygen_add_docs(
    doxygen
    ${PROJECT_SOURCE_DIR}
    COMMENT "Generate XML documentation"
  )
else(DOXYGEN_FOUND)
    message("Doxygen need to be installed to generate the doxygen documentation")
endif(DOXYGEN_FOUND)
