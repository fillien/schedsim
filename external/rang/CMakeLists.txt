add_library(rang INTERFACE)
message(STATUS "RANG LIBRARY")
target_include_directories(rang INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(rang INTERFACE cxx_std_23)
