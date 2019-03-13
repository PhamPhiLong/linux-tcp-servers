/*
 * Copyright (C) Pham Phi Long
 * Created on 3/11/19.
 */

#ifndef LINUX_TCP_SERVERS_SERVER_UTILITY_H
#define LINUX_TCP_SERVERS_SERVER_UTILITY_H

#include <string>
#include <iostream>

namespace tcpserver {
    void log_client_info(const struct sockaddr_storage& cli_addr);
    tcpserver::file_descriptor setup_server_tcp_socket(const std::string& port_num, const int backlog, bool is_nonblock = false);
}

#endif /* LINUX_TCP_SERVERS_SERVER_UTILITY_H */