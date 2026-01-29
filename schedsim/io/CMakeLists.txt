add_library(schedsim-io STATIC)

target_compile_features(schedsim-io PUBLIC cxx_std_20)

# Library 3 depends ONLY on Library 1 (not Library 2)
target_link_libraries(schedsim-io PUBLIC schedsim-core)

# RapidJSON dependency (header-only library)
find_package(RapidJSON QUIET)
if(RapidJSON_FOUND)
    # Different package finders set different variables
    if(RAPIDJSON_INCLUDE_DIRS)
        target_include_directories(schedsim-io PRIVATE ${RAPIDJSON_INCLUDE_DIRS})
    elseif(RapidJSON_INCLUDE_DIRS)
        target_include_directories(schedsim-io PRIVATE ${RapidJSON_INCLUDE_DIRS})
    elseif(RAPIDJSON_INCLUDE_DIR)
        target_include_directories(schedsim-io PRIVATE ${RAPIDJSON_INCLUDE_DIR})
    else()
        # Fallback: try the common location
        find_path(RAPIDJSON_INCLUDE_DIR_FOUND rapidjson/document.h)
        if(RAPIDJSON_INCLUDE_DIR_FOUND)
            target_include_directories(schedsim-io PRIVATE ${RAPIDJSON_INCLUDE_DIR_FOUND})
        endif()
    endif()
else()
    include(FetchContent)
    FetchContent_Declare(
        rapidjson
        GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
        GIT_TAG master  # Use master for C++17/C++20/C++23 compatibility fixes
    )
    FetchContent_MakeAvailable(rapidjson)
    target_include_directories(schedsim-io PRIVATE ${rapidjson_SOURCE_DIR}/include)
endif()

target_sources(schedsim-io PRIVATE
    src/trace_writers.cpp
    src/platform_loader.cpp
    src/scenario_loader.cpp
    src/scenario_injection.cpp
    src/scenario_generation.cpp
    src/metrics.cpp
)

target_include_directories(schedsim-io PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(SCHEDSIM_BUILD_TESTS)
    add_subdirectory(tests)
endif()
