cmake_minimum_required(VERSION 3.20)
project(LetsGoServer)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
# Append the local grpc version to be found using find_packages()
# "/home/jeremiah/ProgrammingAccessories/grpc_1_46_3"
list(APPEND CMAKE_PREFIX_PATH $ENV{CMAKE_PREFIX_PATH_LETS_GO})
set(CMAKE_CXX_STANDARD 20)
#find_package(OpenSSL 1.1.0 REQUIRED)

message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")
message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")

# NOTE: The Order these are added in is important.
set(SERVER_SPECIFIC_PROTO_FILE_DIRECTORY $ENV{SERVER_SPECIFIC_PROTO_FILE_DIRECTORY_LETS_GO})# "/home/jeremiah/Documents/Lets_Go_Docs/lets_go_proto/proto_server_specific")
set(CLIENT_SERVER_PROTO_FILE_DIRECTORY $ENV{CLIENT_SERVER_PROTO_FILE_DIRECTORY_LETS_GO})#"/home/jeremiah/Documents/Lets_Go_Docs/lets_go_proto/proto_client_server_shared")

# Used to include proto_client_server_shared inside of proto_server_specific .proto files
set(SERVER_SPECIFIC_PROTO_INCLUDE_PATHS $ENV{SERVER_SPECIFIC_PROTO_INCLUDE_PATHS_LETS_GO})#"/home/jeremiah/CLionProjects/LetsGoServer/cmake-build-debug/generated_proto_client_server_shared")

# NOTE: The Order these are added in is important. client_server_shared must be generated first.
add_subdirectory(server)
add_subdirectory(${SERVER_SPECIFIC_PROTO_FILE_DIRECTORY} generated_proto_server_specific)
add_subdirectory(${CLIENT_SERVER_PROTO_FILE_DIRECTORY} generated_proto_client_server_shared)
