add_library(protocols)
target_compile_features(protocols PUBLIC cxx_std_23)
target_link_libraries(protocols PUBLIC json)
target_sources(protocols PUBLIC
  src/scenario.cpp
  src/platform.cpp
  src/traces.cpp)
target_include_directories(protocols PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
