add_library(schedsim-io STATIC)

target_compile_features(schedsim-io PUBLIC cxx_std_20)

target_link_libraries(schedsim-io PUBLIC schedsim-core schedsim-algo)

# RapidJSON dependency
find_package(RapidJSON QUIET)
if(NOT RapidJSON_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        rapidjson
        GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
        GIT_TAG v1.1.0
    )
    FetchContent_MakeAvailable(rapidjson)
endif()

target_sources(schedsim-io PRIVATE
    src/io.cpp
)

target_include_directories(schedsim-io PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(SCHEDSIM_BUILD_TESTS)
    add_subdirectory(tests)
endif()
