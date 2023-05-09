cmake_minimum_required(VERSION 3.10.2)
include(FetchContent)

set(CMAKE_BUILD_TYPE Debug)

project("Schedsim"
  VERSION 0.1
  DESCRIPTION "A scheduling simulator")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -Wno-unused")

# Color ninja output
add_compile_options(-fdiagnostics-color=always -fsanitize=address)
#add_compile_options(-fdiagnostics-color=always)
add_link_options(-fsanitize=address)

# Fetch dependancies
FetchContent_Declare(yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG        0579ae3 # 0.7
)
FetchContent_MakeAvailable(yaml-cpp)

# Configure documentation generator
find_package(Doxygen
  REQUIRED dot)

if(DOXYGEN_FOUND)
  set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doc)
  set(DOXYGEN_EXCLUDE_PATTERNS build scenario)
  set(DOXYGEN_USE_MDFILE_AS_MAINPAGE README.md)
  set(DOXYGEN_GENERATE_TREEVIEW YES)
  set(DOXYGEN_DISABLE_INDEX NO)
  set(DOXYGEN_FULL_SIDEBAR NO)
  set(DOXYGEN_HTML_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/doc/doxygen/header.html)
  set(DOXYGEN_HTML_EXTRA_STYLESHEET
    ${CMAKE_CURRENT_SOURCE_DIR}/doc/doxygen/doxygen-awesome.css
    ${CMAKE_CURRENT_SOURCE_DIR}/doc/doxygen/doxygen-awesome-sidebar-only.css
    ${CMAKE_CURRENT_SOURCE_DIR}/doc/doxygen/doxygen-awesome-sidebar-only-darkmode-toggle.css
  )
  set(DOXYGEN_HTML_EXTRA_FILES ${CMAKE_CURRENT_SOURCE_DIR}/doc/doxygen/doxygen-awesome-darkmode-toggle.js)
  set(DOXYGEN_HTML_COLORSTYLE LIGHT)
  set(DOXYGEN_BUILTIN_STL_SUPPORT YES)
  set(DOXYGEN_CPP_CLI_SUPPORT YES)
  set(DOXYGEN_EXTRACT_ALL YES)
  set(DOXYGEN_EXTRACT_PRIVATE YES)
  set(DOXYGEN_UML_LOOK YES)
  set(DOXYGEN_DOT_UML_DETAILS YES)
  set(DOXYGEN_HAVE_DOT YES)
  set(DOXYGEN_DOT_IMAGE_FORMAT svg)
  set(DOXYGEN_TRANSPARENT YES)
  set(DOXYGEN_TAB_SIZE 8)
  set(DOXYGEN_GENERATE_TODOLIST YES)
  
  doxygen_add_docs(
    doxygen
    ${PROJECT_SOURCE_DIR}
    COMMENT "Generate HTML documentation"
  )
else(DOXYGEN_FOUND)
  message("Doxygen need to be installed to generate the doxygen documentation")
endif(DOXYGEN_FOUND)

file(GLOB source
  "main.cpp"
  "engine.cpp"
  "event.cpp"
  "plateform.cpp"
  "processor.cpp"
  "scheduler.cpp"
  "server.cpp"
  "tracer.cpp"
  "task.cpp"
)

add_executable(launch ${source})
target_link_libraries(launch PRIVATE yaml-cpp)
