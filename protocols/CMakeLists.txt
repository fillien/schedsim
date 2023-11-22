find_package(Protobuf REQUIRED)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS logs.proto)

add_library(protocols ${PROTO_SRCS} ${PROTO_HDRS})
target_include_directories(protocols PUBLIC ${CMAKE_BINARY_DIR})
target_include_directories(protocols PUBLIC ${Protobuf_INCLUDE_DIRS})
target_link_libraries(protocols PUBLIC ${Protobuf_LIBRARIES})
