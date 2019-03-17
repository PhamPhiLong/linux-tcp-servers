#include <utility>

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

#ifndef LINUX_TCP_SERVERS_MULTI_WORKER_REUSEPORT_NONBLOCKING_MULTIPLEXING_IO_SERVER_H
#define LINUX_TCP_SERVERS_MULTI_WORKER_REUSEPORT_NONBLOCKING_MULTIPLEXING_IO_SERVER_H

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/epoll.h>
#include <array>
#include <sys/wait.h>

#include "utilities/print_utility.h"
#include "utilities/file_descriptor.h"
#include "utilities/server_utility.h"
#include "include/constants.h"


namespace concurrent_servers {
    template <typename ReadHandler>
    class linux_concurrent_server {
    public:
        explicit
        linux_concurrent_server(const int worker_process_num,
                std::string port_num,
                const int backlog
                ) : _worker_process_num{worker_process_num},
                    _port_num{std::move(port_num)},
                    _backlog{backlog}
        {}

        void start() const {
            concurrent_servers::file_descriptor server_sfd;

//            const size_t process_name_len{strlen(argv[0])};
//            strncpy(argv[0], "tcp-server master process", process_name_len); /* Change process name */
//            concurrent_servers::log_info(argv[0]);           // Print process name
//            strncpy(argv[0], "tcp-server worker process pid=", process_name_len); /* Change process name */

            // Pre-fork worker processes to distribute accept() and read()
            // for accept(), the server listening socket fd
            for (int i{0}; i < _worker_process_num; ++ i) {
                int child_pid;

                if ((child_pid = fork()) == -1) { // failed to fork a child process
                    throw std::runtime_error("Failed to fork worker processes");
                } else if (child_pid > 0) { // parent process
                    std::cout << "\033[32m" << "Forked new worker process with pid=" << child_pid << "\033[0m" << std::endl;
                } else if (child_pid == 0) { // child process
                    try {
                        server_sfd = concurrent_servers::setup_server_tcp_socket(_port_num, _backlog, true, true);

                        // create the epoll socket
                        concurrent_servers::file_descriptor epoll_fd;
                        if (!epoll_fd.set_fd(epoll_create1(0))) {
                            throw std::runtime_error("epoll_create1() failed");
                        }

                        // mark the server socket for reading, and become edge-triggered
                        struct epoll_event event{};
                        memset(&event, 0, sizeof(event));
                        event.data.fd = server_sfd.get_fd();
                        event.events = EPOLLIN | EPOLLET;
                        if (epoll_ctl(epoll_fd.get_fd(), EPOLL_CTL_ADD, server_sfd.get_fd(), &event) == -1) {
                            throw std::runtime_error("epoll_ctl() failed");
                        }

                        epoll_event_loop(server_sfd, epoll_fd);

                        wait(nullptr);
                    } catch (const std::runtime_error& e) {
                        concurrent_servers::log_error(e.what(), "\n\t", strerror(errno));
                        server_sfd.close_fd();
                        exit(EXIT_FAILURE);
                    }
                }
            }
        }

    private:
        const int _worker_process_num;
        const std::string _port_num;
        const int _backlog;
        const ReadHandler _read_handler{};

        void epoll_event_loop(const concurrent_servers::file_descriptor& server_sfd, const concurrent_servers::file_descriptor& epoll_fd) const {
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
                                }

                                _read_handler(prefix_log, buffer, rlen);
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

    };
}

#endif //LINUX_TCP_SERVERS_MULTI_WORKER_REUSEPORT_NONBLOCKING_MULTIPLEXING_IO_SERVER_H
