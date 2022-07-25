cmake_minimum_required(VERSION 3.13)
project(linux_tcp_servers)

set(CMAKE_CXX_STANDARD 17)

include_directories(src src/include src/utilities)

add_executable(linux_tcp_servers
        src/servers/server_main.cpp
        src/servers/multi_worker_nonblocking_io_multiplexing_server.h
        src/utilities/file_descriptor.h
        src/utilities/constants.cpp)

add_executable(linux_tcp_client
        src/clients/echo_client.cpp
        src/utilities/print_utility.h
        src/utilities/constants.cpp)
