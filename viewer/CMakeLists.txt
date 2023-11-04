# Color ninja output
#add_compile_options(-fdiagnostics-color=always -fsanitize=address)
#add_compile_options(-fdiagnostics-color=always)
#add_link_options(-fsanitize=address)

add_executable(viewer)

target_sources(viewer PRIVATE main.cpp)
target_link_libraries(viewer PRIVATE nlohmann_json)
add_subdirectory(inputs)
add_subdirectory(outputs)
