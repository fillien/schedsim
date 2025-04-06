add_executable(schedview)

target_compile_features(schedview PRIVATE cxx_std_23)
target_compile_options(schedview PRIVATE -fdiagnostics-color=always)

# Set Tracy option based on build type
if (ENABLE_TRACY)
    message(STATUS "Tracy is enabled")
    set(ENABLE_TRACY ON)
    FetchContent_Declare(tracy
 GIT_REPOSITORY https://github.com/wolfpld/tracy.git
 GIT_TAG v0.10
 GIT_SHALLOW TRUE
 GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(tracy)
    target_link_libraries(schedview PRIVATE TracyClient cxxopts protocols)
else()
    message(STATUS "Tracy is disabled")
    set(ENABLE_TRACY OFF)
    target_link_libraries(schedview PRIVATE cxxopts protocols)
endif()

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
  src/system_utilization.hpp
  src/system_utilization.cpp
)
