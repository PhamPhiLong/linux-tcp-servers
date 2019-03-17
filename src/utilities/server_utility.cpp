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
#include <sys/epoll.h>
#include <array>

#include "print_utility.h"
#include "file_descriptor.h"
#include "constants.h"

namespace concurrent_servers {
    void log_client_info(const struct sockaddr_storage& cli_addr, const std::string &prefix_log) {
        switch (cli_addr.ss_family) {
            case AF_INET:   /* IPv4 address. */ {
                char addr_str[INET_ADDRSTRLEN];
                auto *p = (struct sockaddr_in *) &cli_addr;
                concurrent_servers::log_info(prefix_log, "New client connected: address=", inet_ntop(p->sin_family, &p->sin_addr, addr_str, INET_ADDRSTRLEN), ", port=", p->sin_port);
                break;
            }
            case AF_INET6:   /* IPv6 address. */ {
                char addr_str[INET6_ADDRSTRLEN];
                auto *p = (struct sockaddr_in6 *) &cli_addr;
                concurrent_servers::log_info(prefix_log, "New client connected: address=", inet_ntop(p->sin6_family, &p->sin6_addr, addr_str, INET_ADDRSTRLEN), ", port=", p->sin6_port);;
                break;
            }
            default: {}
        }
    }

    concurrent_servers::file_descriptor setup_server_tcp_socket(const std::string& port_num, const int backlog, bool is_nonblock, bool reuse_port) {
        concurrent_servers::file_descriptor server_sfd{};

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

            if (reuse_port) {
                int one = 1;
                setsockopt(server_sfd.get_fd(), SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &one, sizeof(one));
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

    void epoll_event_loop(const concurrent_servers::file_descriptor& server_sfd, const concurrent_servers::file_descriptor& epoll_fd) {
        const pid_t pid = getpid();
        const std::string prefix_log = "Worker process " + std::to_string(pid) + ": ";
        const int MAX_EVENTS{10000};
        std::array<struct epoll_event, MAX_EVENTS> events{};

        for (;;) {
            int nfds = epoll_wait(epoll_fd.get_fd(), events.data(), MAX_EVENTS, -1);
            if (nfds == -1) {
                throw std::runtime_error(prefix_log + "epoll_wait() failed");
            }

            for (int i{0}; i < nfds; ++i) {
                concurrent_servers::log_info(prefix_log, "epoll_wait() return, fd=", events[i].data.fd, " event ", events[i].events);

                if (events[i].events & EPOLLERR) {
                    concurrent_servers::log_warning(prefix_log + "epoll_wait() error on fd ", events[i].data.fd, " event ", events[i].events);
//                    epoll_ctl(epoll_fd.get_fd(), EPOLL_CTL_DEL, events[i].data.fd, nullptr);
//                    close(events[i].data.fd);
//                    if (events[i].data.fd == server_sfd.get_fd()) {
//                        throw std::runtime_error(prefix_log + "epoll_wait() error on server fd");
//                    }
                }

                if (events[i].events & EPOLLRDHUP) {
                    concurrent_servers::log_warning(prefix_log, "  Connection closed, fd=", events[i].data.fd);
                    epoll_ctl(epoll_fd.get_fd(), EPOLL_CTL_DEL, events[i].data.fd, nullptr); // remove client socket fd from epoll list
                    close(events[i].data.fd);
                    continue;
                }

                if (events[i].events & EPOLLHUP) {
                    concurrent_servers::log_warning(prefix_log, "  Connection hangup");
                }

                if (events[i].events & EPOLLIN) {
                    concurrent_servers::log_info(prefix_log, "  EPOLLIN event, fd=", events[i].data.fd);

                    if (events[i].data.fd == server_sfd.get_fd()) {
                        // server socket, accept as many new connections as possible
                        concurrent_servers::file_descriptor client_sfd{};
                        struct sockaddr_storage cli_addr{};

                        for (;;) {
                            int cli_len = sizeof(cli_addr); // Always reset this value before calling accept()
                            if ((!client_sfd.set_fd(
                                    accept4(server_sfd.get_fd(), (struct sockaddr *) &cli_addr, (socklen_t *) &cli_len,
                                            SOCK_NONBLOCK)))) {
                                if (errno != EWOULDBLOCK and errno != EAGAIN) {
                                    throw std::runtime_error(prefix_log + "Could not accept a new connection");
                                } else {
                                    // no more connection to accept
                                    break;
                                }
                            }

                            log_client_info(cli_addr, prefix_log);
                            concurrent_servers::log_info(prefix_log + "Add new client socket fd=", client_sfd.get_fd());

                            // add the new client fd to epoll event list
                            struct epoll_event event{};
                            memset(&event, 0, sizeof(event));
                            event.data.fd = client_sfd.get_fd();
                            event.events = EPOLLIN | EPOLLRDHUP |EPOLLET | EPOLLONESHOT;  // one shot edge triggered
                            if (epoll_ctl(epoll_fd.get_fd(), EPOLL_CTL_ADD, client_sfd.get_fd(), &event) == -1) {
                                throw std::runtime_error(prefix_log + "epoll_ctl() failed");
                            }
                        }
                    } else {
                        // client socket; read as much data as we can
                        concurrent_servers::file_descriptor client_sfd{events[i].data.fd};
                        char buffer[BUFF_SIZE];

                        for (;;) {
                            // Read data sent from client
                            const ssize_t rlen = read(client_sfd.get_fd(), buffer, sizeof(buffer));
                            if (rlen < 0) {
                                if (errno == EWOULDBLOCK or errno == EAGAIN) {
                                    // due to EPOLLONESHOT, after finishing reading all data in buffer,
                                    // we need to rearm the client fd to catch its event again
                                    concurrent_servers::log_info(prefix_log + "rearm epoll event, fd=", client_sfd.get_fd());
                                    struct epoll_event event{};
                                    memset(&event, 0, sizeof(event));
                                    event.data.fd = client_sfd.get_fd();
                                    event.events = EPOLLIN | EPOLLRDHUP | EPOLLET | EPOLLONESHOT;  // one shot edge triggered
                                    if (epoll_ctl(epoll_fd.get_fd(), EPOLL_CTL_MOD, client_sfd.get_fd(), &event) == -1) {
                                        throw std::runtime_error(prefix_log + "epoll_ctl() failed");
                                    }
                                } else {
                                    concurrent_servers::log_error(prefix_log, "error on reading, fd=",
                                                         std::to_string(client_sfd.get_fd()), ", errno=",
                                                         std::to_string(errno), "\t", strerror(errno));
//                                    throw std::runtime_error(prefix_log + "ERROR on reading, fd=" + std::to_string(client_sfd.get_fd()) + ", errno=" + std::to_string(errno));
                                }
                                break;
                            }

                            if (rlen == 0) {
                                concurrent_servers::log_info(prefix_log, "  end of file, fd=" + std::to_string(client_sfd.get_fd()));
                                break;
                            }

                            concurrent_servers::log_info(prefix_log, "  received: ", buffer);
                        }
                    }
                }

                if (events[i].events & EPOLLOUT) {
                    concurrent_servers::log_info(prefix_log, "  EPOLLOUT event, fd=", events[i].data.fd);

//                    concurrent_servers::file_descriptor client_sfd{events[i].data.fd};
//                    const std::string dummy_buffer{"echo back"};
//
//
//                    // Echo the data back to the client
//                    const ssize_t wlen = write(client_sfd.get_fd(), dummy_buffer.data(), dummy_buffer.length());
//                    if (wlen < 0) {
//                        if (errno != EWOULDBLOCK and errno != EAGAIN) {
//                            concurrent_servers::log_error(prefix_log, "ERROR on writing, fd=", std::to_string(client_sfd.get_fd()), ", errno=", std::to_string(errno), "\t", strerror(errno));
//                            throw std::runtime_error(prefix_log + "ERROR on writing, fd=" + std::to_string(client_sfd.get_fd()) + ", errno=" + std::to_string(errno));
//                        }
//                        break;
//                    }
                }
            }
        }
    }
}
