add_library(scenario)

target_compile_features(scenario PUBLIC cxx_std_23)
#target_compile_options(scenario PRIVATE -fdiagnostics-color=always -fsanitize=address)
#target_link_options(scenario PRIVATE -fsanitize=address)
target_link_libraries(scenario PUBLIC json)
target_sources(scenario PUBLIC scenario.hpp PRIVATE scenario.cpp)
target_include_directories(scenario PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
