cmake_minimum_required(VERSION 3.13)
project(linux_tcp_servers)

set(CMAKE_CXX_STANDARD 14)

include_directories(src src/include src/utilities)

add_executable(linux_tcp_servers
        src/servers/single_client_blocking_io_server.cpp
        src/servers/nonblocking_io_server.cpp
        src/servers/fork_process_per_connection_server.cpp
        src/servers/nonblocking_io_multiplexing_select_server.cpp
        src/servers/nonblocking_io_multiplexing_edge_trigger_epoll_server.cpp
        src/clients/blocking_client.cpp
        src/utilities/server_utility.h
        src/utilities/server_utility.cpp
        src/utilities/file_descriptor.h)
