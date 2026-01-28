add_library(schedsim-algo STATIC)

target_compile_features(schedsim-algo PUBLIC cxx_std_20)

target_link_libraries(schedsim-algo PUBLIC schedsim-core)

target_sources(schedsim-algo PRIVATE
    src/algo.cpp
)

target_include_directories(schedsim-algo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(SCHEDSIM_BUILD_TESTS)
    add_subdirectory(tests)
endif()
