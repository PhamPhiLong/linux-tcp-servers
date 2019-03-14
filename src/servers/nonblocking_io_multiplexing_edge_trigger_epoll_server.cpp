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


#include <string.h>
#include <sys/socket.h>
#include <array>
#include <sys/epoll.h>

#include "constants.h"
#include "print_utility.h"
#include "file_descriptor.h"
#include "server_utility.h"

int main(int argc, char *argv[]) {
    tcpserver::log(argv[0]);           // Print server process name

    const std::string port_num = (argc >= 2) ? argv[1] : tcpserver::DEFAULT_PORT;
    const int backlog = (argc < 3) ? DEFAULT_BACKLOG : atoi(argv[2]);
    tcpserver::file_descriptor server_sfd;

    try {
        server_sfd = tcpserver::setup_server_tcp_socket(port_num, backlog, true);

        // create the epoll socket
        tcpserver::file_descriptor epoll_fd;
        if (!epoll_fd.set_fd(epoll_create1(0))) {
            throw std::runtime_error("epoll_create1() failed");
        }

        // mark the server socket for reading, and become edge-triggered
        struct epoll_event event{};
        memset(&event, 0, sizeof(event));
        event.data.fd = server_sfd.get_fd();
        event.events = EPOLLIN | EPOLLET; // edge-triggered
        if (epoll_ctl(epoll_fd.get_fd(), EPOLL_CTL_ADD, server_sfd.get_fd(), &event) == -1) {
            throw std::runtime_error("epoll_ctl() failed");
        }

        const int MAX_EVENTS{10000};
        std::array<struct epoll_event, MAX_EVENTS> events{};

        for (;;) {
            int nfds = epoll_wait(epoll_fd.get_fd(), events.data(), MAX_EVENTS, -1);
            if (nfds == -1) {
                throw std::runtime_error("epoll_wait() failed");
            }

            for (int i{0}; i < nfds; ++i) {
                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    tcpserver::log("epoll_wait() error on fd ", events[i].data.fd, " event ", events[i].events);
                    close(events[i].data.fd);
                    continue;
                } else if (events[i].data.fd == server_sfd.get_fd()) {
                    // server socket, accept as many new connections as possible
                    tcpserver::file_descriptor client_sfd{};
                    struct sockaddr_storage cli_addr{};

                    for (;;) {
                        int cli_len = sizeof(cli_addr); // Always reset this value before calling accept()
                        if ((!client_sfd.set_fd(
                                accept4(server_sfd.get_fd(), (struct sockaddr *) &cli_addr, (socklen_t *) &cli_len, SOCK_NONBLOCK)))) {
                            if (errno != EWOULDBLOCK and errno != EAGAIN) {
                                throw std::runtime_error("Could not accept a new connection");
                            } else {
                                // no more connection to accept
                                break;
                            }
                        }

                        tcpserver::log_client_info(cli_addr);
                        tcpserver::log("Add new client socket fd ", client_sfd.get_fd());
                        event.data.fd = client_sfd.get_fd();
                        event.events = EPOLLIN | EPOLLET; // edge-triggered
                        if (epoll_ctl(epoll_fd.get_fd(), EPOLL_CTL_ADD, client_sfd.get_fd(), &event) == -1) {
                            throw std::runtime_error("epoll_ctl() failed");
                        }
                    }
                } else {
                    // client socket; read as much data as we can
                    char buffer[BUFF_SIZE];

                    for (;;) {
                        // Read data sent from client
                        const ssize_t rlen = read(events[i].data.fd, buffer, sizeof(buffer));
                        if (rlen < 0) {
                            if (errno != EWOULDBLOCK and errno != EAGAIN) {
                                throw std::runtime_error("ERROR on reading");
                            }
                            break;
                        }

                        if (rlen == 0) {
                            std::cout << "  Connection closed" << std::endl;
                            close(events[i].data.fd);
                            break;
                        }

                        std::cout << "  received: " << buffer << std::endl;

                        // Echo the data back to the client
                        const ssize_t wlen = write(events[i].data.fd, buffer, (size_t)rlen);
                        if (wlen < 0) {
                            if (errno != EWOULDBLOCK and errno != EAGAIN) {
                                throw std::runtime_error("ERROR on  writing");
                            }
                            break;
                        }
                    }
                }
            }
        }
    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        server_sfd.close_fd();
        exit(EXIT_FAILURE);
    }
}