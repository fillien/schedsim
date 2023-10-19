# Color ninja output
add_compile_options(-fdiagnostics-color=always -fsanitize=address)
#add_compile_options(-fdiagnostics-color=always)
add_link_options(-fsanitize=address)

file(GLOB source
  "main.cpp"
  "rtsched.cpp"
  "parse_trace.cpp"
)

add_executable(viewer ${source})
target_link_libraries(viewer PRIVATE nlohmann_json)
