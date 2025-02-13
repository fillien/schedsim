add_library(protocols)
target_compile_features(protocols PUBLIC cxx_std_23)

# Set Tracy option based on build type
if (ENABLE_TRACY)
    message(STATUS "Tracy is enabled")
    set(ENABLE_TRACY ON)
    target_link_libraries(protocols PUBLIC TracyClient json)
else()
    message(STATUS "Tracy is disabled")
    set(ENABLE_TRACY OFF)
    target_link_libraries(protocols PUBLIC json)
endif()

target_sources(protocols PUBLIC
  src/scenario.cpp
  src/hardware.cpp
  src/traces.cpp)
target_include_directories(protocols PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
