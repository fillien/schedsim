add_library(schedsim-core STATIC)

target_compile_features(schedsim-core PUBLIC cxx_std_20)

target_sources(schedsim-core PRIVATE
    src/core.cpp
)

target_include_directories(schedsim-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Tests
if(SCHEDSIM_BUILD_TESTS)
    add_subdirectory(tests)
endif()
