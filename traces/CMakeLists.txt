add_library(traces)

target_compile_features(traces PUBLIC cxx_std_23)
#target_compile_options(traces PRIVATE -fdiagnostics-color=always -fsanitize=address)
#target_link_options(traces PRIVATE -fsanitize=address)
target_link_libraries(traces PUBLIC json)
target_sources(traces PUBLIC traces.hpp PRIVATE traces.cpp)
target_include_directories(traces PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
