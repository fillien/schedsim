add_executable(schedview)

target_compile_features(schedview PRIVATE cxx_std_23)
target_compile_options(schedview PRIVATE -fdiagnostics-color=always -fsanitize=address)
target_link_options(schedview PRIVATE -fsanitize=address)

target_sources(schedview PRIVATE
  src/main.cpp
  src/gantt/gantt.cpp
  src/gantt/gantt.hpp
  src/gantt/svg.cpp
  src/gantt/svg.hpp
  src/gantt/rtsched.cpp
  src/gantt/rtsched.hpp
  src/textual.cpp
  src/textual.hpp
  src/energy.hpp
  src/energy.cpp
  src/energy_model.hpp
  src/energy_model.cpp
  src/frequency.hpp
  src/frequency.cpp
  src/stats.hpp
  src/stats.cpp
  src/deadline_misses.hpp
  src/deadline_misses.cpp
)

target_link_libraries(schedview PRIVATE protocols cxxopts)
