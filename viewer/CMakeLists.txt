FetchContent_Declare(cxxopts
  GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
  GIT_TAG        eb78730
)
FetchContent_MakeAvailable(cxxopts)

FetchContent_Declare(matplotplusplus
  GIT_REPOSITORY https://github.com/alandefreitas/matplotplusplus
  GIT_TAG e32cabd29c941f3b18f27cf086fceb9978988a3d
)

FetchContent_GetProperties(matplotplusplus)
if(NOT matplotplusplus_POPULATED)
    FetchContent_Populate(matplotplusplus)
    add_subdirectory(${matplotplusplus_SOURCE_DIR} ${matplotplusplus_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

add_executable(viewer)

target_compile_features(viewer PRIVATE cxx_std_23)
target_compile_options(viewer PRIVATE -fdiagnostics-color=always -fsanitize=address)
target_link_options(viewer PRIVATE -fsanitize=address)

target_sources(viewer PRIVATE
  src/main.cpp
  src/rtsched.cpp
  src/rtsched.hpp
  src/textual.cpp
  src/textual.hpp
  src/energy.hpp
  src/energy.cpp
  src/stats.hpp
  src/stats.cpp
  src/deadline_misses.hpp
  src/deadline_misses.cpp
)

target_link_libraries(viewer PRIVATE traces matplot cxxopts)
