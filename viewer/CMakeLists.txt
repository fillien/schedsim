add_executable(viewer)

target_compile_features(viewer PRIVATE cxx_std_23)
#target_compile_options(viewer PRIVATE -fdiagnostics-color=always -fsanitize=address)
#target_link_options(viewer PRIVATE -fsanitize=address)

target_sources(viewer PRIVATE
  src/main.cpp
  src/rtsched.cpp
  src/rtsched.hpp
  src/textual.cpp
  src/textual.hpp
)

target_link_libraries(viewer PRIVATE traces)
