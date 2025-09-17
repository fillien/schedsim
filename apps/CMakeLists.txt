function(add_app TARGET_NAME SOURCE_FILE)
    add_executable(${TARGET_NAME})
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_23)

    # if (NOT APPLE)
    #     target_compile_options(${TARGET_NAME} PRIVATE
    #         -fdiagnostics-color=always
    #         -fsanitize=address
    #     )
    #     target_link_options(${TARGET_NAME} PRIVATE
    #         -fsanitize=address
    #     )
    # endif()

    target_sources(${TARGET_NAME} PRIVATE ${SOURCE_FILE})
    target_link_libraries(${TARGET_NAME} cxxopts schedlib)
endfunction()

add_app(schedgen generator.cpp)
add_app(schedsim simulator.cpp)
add_app(schedview analyzer.cpp)

find_package(OpenMP REQUIRED)
add_executable(optimal)
target_compile_features(optimal PRIVATE cxx_std_23)
target_sources(optimal PRIVATE optimal.cpp)
target_link_libraries(optimal cxxopts schedlib OpenMP::OpenMP_CXX)

add_executable(alloc)
target_compile_features(alloc PRIVATE cxx_std_23)
target_sources(alloc PRIVATE alloc.cpp)
target_link_libraries(alloc cxxopts schedlib OpenMP::OpenMP_CXX)
