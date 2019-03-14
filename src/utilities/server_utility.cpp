/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2019, Pham Phi Long <phamphilong2010@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>

#include "print_utility.h"
#include "file_descriptor.h"

namespace tcpserver {
    void log_client_info(const struct sockaddr_storage& cli_addr) {
        tcpserver::log("New client connected: ");
        switch (cli_addr.ss_family) {
            case AF_INET:   /* IPv4 address. */ {
                char addr_str[INET_ADDRSTRLEN];
                auto *p = (struct sockaddr_in *) &cli_addr;
                fprintf(stdin, "%s:%d", inet_ntop(p->sin_family, &p->sin_addr, addr_str, INET_ADDRSTRLEN), p->sin_port);
                break;
            }
            case AF_INET6:   /* IPv6 address. */ {
                char addr_str[INET6_ADDRSTRLEN];
                auto *p = (struct sockaddr_in6 *) &cli_addr;
                fprintf(stdin, "%s:%d", inet_ntop(p->sin6_family, &p->sin6_addr, addr_str, INET_ADDRSTRLEN), p->sin6_port);;
                break;
            }
            default: {}
        }
    }

    tcpserver::file_descriptor setup_server_tcp_socket(const std::string& port_num, const int backlog, bool is_nonblock) {
        tcpserver::file_descriptor server_sfd{};

        // Setup server address
        struct addrinfo hints{};
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
        hints.ai_socktype = SOCK_STREAM; // TCP socket
        hints.ai_flags = AI_PASSIVE;     // For wildcard IP address
        hints.ai_protocol = 0;           // Any protocol
        hints.ai_canonname = nullptr;
        hints.ai_addr = nullptr;
        hints.ai_next = nullptr;

        struct addrinfo *result{nullptr}, *rp{nullptr};
        const int s = getaddrinfo(nullptr, port_num.data(), &hints, &result);
        if (s != 0) {
            throw std::runtime_error("getaddrinfo: " + std::string{gai_strerror(s)});
        }

        /* getaddrinfo() returns a list of address structures.
         * Try each address until we successfully bind(2).
         * If socket(2) (or bind(2)) fails, we (close the socket
         *  and) try the next address.
         */
        for (rp = result; rp != nullptr; rp = rp->ai_next) {
            const int socket_type = is_nonblock ? (rp->ai_socktype | SOCK_NONBLOCK) : rp->ai_socktype;

            if (!server_sfd.set_fd(socket(rp->ai_family, socket_type, rp->ai_protocol))) {
                continue;
            }

            if (bind(server_sfd.get_fd(), rp->ai_addr, rp->ai_addrlen) == 0) {
                break;                  // Success
            }

            server_sfd.close_fd();
        }

        if (rp == nullptr) {               // No address succeeded
            throw std::runtime_error("Could not bind");
        }

        freeaddrinfo(result);           // No longer needed

        if (listen(server_sfd.get_fd(), backlog) < 0) {  // Listen to the socket
            server_sfd.close_fd();
            throw std::runtime_error("Could not listen to port " + port_num);
        }

        return server_sfd;
    }
}
