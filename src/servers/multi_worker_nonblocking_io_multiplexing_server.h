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


#include <cstring>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/prctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <array>
#include <vector>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <mutex>

#include "constants.h"
#include "print_utility.h"
#include "file_descriptor.h"

#define PREFIX_LOG prefix_log_, "line ", __LINE__, ":\t"

class MultiWorkerIoMultiplexingTCPServer {
public:
    MultiWorkerIoMultiplexingTCPServer(std::string port_num, int backlog, int worker_num) :
        server_sfd_{-1},
        port_num_{std::move(port_num)},
        backlog_{backlog},
        worker_num_{worker_num},
        data_manager_{} {

    }

    void start() {
        try {
            server_sfd_ = setupServerTcpSocket(port_num_, backlog_, true, false);
            concurrent_servers::log_info("\033[32m", "server socket fd=", server_sfd_, "\033[0m");

            // create the epoll socket
            int epoll_fd = epoll_create1(0);
            if (epoll_fd < 0) {
                throw std::runtime_error("epoll_create1() failed");
            }

            // mark the server socket for reading, and become edge-triggered
            struct epoll_event event{};
            memset(&event, 0, sizeof(event));
            event.data.ptr = data_manager_.insert(server_sfd_);
            event.events = EPOLLIN | EPOLLEXCLUSIVE; // use level-triggered and EPOLLEXCLUSIVE to distribute accept() to
                                                     // multiple threads or processes
                                                     // Reference:
                                                     //     https://idea.popcount.org/2017-02-20-epoll-is-fundamentally-broken-12/
                                                     //     https://sudonull.com/post/14030-The-whole-truth-about-linux-epoll
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_sfd_, &event) == -1) {
                throw std::runtime_error("epoll_ctl() failed");
            }

            // Pre-fork worker processes to distribute accept() and read()
            // for accept(), the server listening socket fd
            for (int i{0}; i < worker_num_; ++ i) {
                int child_pid;

                if ((child_pid = fork()) == -1) { // failed to fork a child process
                    throw std::runtime_error("Failed to fork worker processes");
                } else if (child_pid > 0) { // parent process
                    concurrent_servers::log_info("\033[32m", "Forked new worker process with pid=", child_pid, "\033[0m");
                } else if (child_pid == 0) { // child process
                    Worker worker{server_sfd_, epoll_fd, i, this->data_manager_};
                    worker.start();
                }
            }

            bool terminate_server{false};
            std::cin >> terminate_server;
        } catch (const std::runtime_error& e) {
            concurrent_servers::log_error(e.what(), "\t", strerror(errno));
            close(server_sfd_);
            exit(EXIT_FAILURE);
        }
    }

private:
    /**
     * This class is not thread-safe
     */
    struct ConnectionData {
        explicit ConnectionData(int conn_fd) :
                conn_fd_{conn_fd},
                buffer_(1024, '\0'),
                read_index_{0},
                write_index_{0},
                ready_for_write_{false} {
            
        }

        int conn_fd_;
        std::vector<char> buffer_;
        ssize_t read_index_;
        ssize_t write_index_;
        bool ready_for_write_;
    };

    class ConnectionDataManager {
    public:
        ConnectionData *get(int conn_fd) {
            std::shared_lock s_lock(mutex_);
            auto it = data_.find(conn_fd);
            return (it != data_.end()) ? it->second.get() : nullptr;
        }

        ConnectionData *insert(int conn_fd) {
            std::unique_lock u_lock(mutex_);
            auto result = data_.insert({conn_fd, std::make_shared<ConnectionData>(conn_fd)});
            return result.second ? result.first->second.get() : nullptr;
        }

        void remove(int conn_fd) {
            std::unique_lock u_lock(mutex_);
            data_.erase(conn_fd);
        }

    private:
        std::shared_mutex mutex_{};
        std::unordered_map<int, std::shared_ptr<ConnectionData>> data_{};
    };

    class Worker {
    public:
        Worker(int server_sfd, int epoll_fd, int worker_id, ConnectionDataManager &data_manager) :
                server_sfd_{server_sfd},
                epoll_fd_{epoll_fd},
                worker_id_{worker_id},
                data_manager_{data_manager},
                prefix_log_{"Multi Worker Server: Worker " + std::to_string(worker_id_) + ": "},
                event_{} {

        }

        void start() {
            prctl(PR_SET_NAME, prefix_log_.c_str(), NULL, NULL, NULL);

            for (;;) {
                int nfds = epoll_wait(epoll_fd_, events_.data(), MAX_EVENTS, -1);
                if (nfds == -1) {
                    throw std::runtime_error(prefix_log_ + "epoll_wait() failed");
                }

                for (int i{0}; i < nfds; ++i) {
                    auto *conn_data = (ConnectionData *)events_[i].data.ptr;
                    if (conn_data->conn_fd_ == server_sfd_) {
                        concurrent_servers::log_info(PREFIX_LOG, "epoll_wait() returns, fd=", conn_data->conn_fd_, " event ", events_[i].events, " this is the server listening fd");
                        acceptConnections(events_[i], conn_data);
                    } else {
                        concurrent_servers::log_info(PREFIX_LOG, "epoll_wait() returns, fd=", conn_data->conn_fd_, " event ", events_[i].events, " this is a connection fd");

                        if (events_[i].events & EPOLLERR) {
                            concurrent_servers::log_warning(PREFIX_LOG, "epoll_wait() error on fd ", conn_data->conn_fd_, " event ", events_[i].events);
                            closeConnection(epoll_fd_, conn_data->conn_fd_);
                            continue;
                        } else if (events_[i].events & EPOLLRDHUP) {
                            concurrent_servers::log_warning(PREFIX_LOG, "  Connection closed, fd=", conn_data->conn_fd_);
                            closeConnection(epoll_fd_, conn_data->conn_fd_);
                            continue;
                        } else if (events_[i].events & EPOLLHUP) {
                            concurrent_servers::log_info(PREFIX_LOG, "  Connection hangup");
                            continue;
                        }

                        handleConnectionEvent(events_[i], conn_data);
                    }
                }
            }
        }

    private:
        int server_sfd_;
        int epoll_fd_;
        const int worker_id_; // could be either process id or thread id
        static const int MAX_EVENTS{100000};
        std::array<epoll_event, MAX_EVENTS> events_{};
        ConnectionDataManager &data_manager_;
        const std::string prefix_log_;
        struct epoll_event event_;

        void acceptConnections(epoll_event &new_conn_event, ConnectionData *conn_data) {
            if (new_conn_event.events & EPOLLIN) {
                concurrent_servers::log_info(PREFIX_LOG, "  EPOLLIN event, fd=", conn_data->conn_fd_);
                concurrent_servers::log_info(PREFIX_LOG, "  start accepting connections");
            }
            // server socket, accept as many new connections as possible
            struct sockaddr_storage cli_addr{};

            for (;;) {
                int cli_len = sizeof(cli_addr); // Always reset this value before calling accept()
                int conn_fd = accept4(server_sfd_, (struct sockaddr *) &cli_addr, (socklen_t *) &cli_len,SOCK_NONBLOCK);

                if (conn_fd < 0) {
                    if (errno != EWOULDBLOCK and errno != EAGAIN) {
                        concurrent_servers::log_error(PREFIX_LOG, "      could not accept a new connection. ", strerror(errno));
//                                    throw std::runtime_error(prefix_log_ + "Could not accept a new connection");
                    } else {
                        // no more connection to accept
                        concurrent_servers::log_info(PREFIX_LOG, "      no more connection to accept");
                        break;
                    }
                }

                if (not concurrent_servers::is_nonblocking(conn_fd)) {
                    concurrent_servers::log_error(PREFIX_LOG, "      new connection socket is blocking");
                    concurrent_servers::set_nonblocking(conn_fd);
                }

                log_client_info(cli_addr, prefix_log_);
                concurrent_servers::log_info(PREFIX_LOG, "      add new client socket fd=", conn_fd);

                // add the new client fd to epoll event list
                event_.events = EPOLLIN | EPOLLET | EPOLLONESHOT;  // one shot edge triggered
                event_.data.ptr = data_manager_.insert(conn_fd);

                if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, conn_fd, &event_) == -1) {
                    concurrent_servers::log_error(PREFIX_LOG, " epoll_ctl() failed. Could not register event for new client_fd ", conn_fd);
                    break;
                }
            }
        }

        void handleConnectionEvent(epoll_event &conn_event, ConnectionData *conn_data) {
            if (not conn_data->ready_for_write_) {
                concurrent_servers::log_info(PREFIX_LOG, "  handleConnectionEvent() ready for read, connection events: ", conn_event.events);
                if (not (conn_event.events & EPOLLIN)) {
                    return;
                }

                if (not concurrent_servers::is_nonblocking(conn_data->conn_fd_)) {
                    concurrent_servers::log_error(PREFIX_LOG, "      connection socket is blocking");
                }

                for (;;) {
                    concurrent_servers::log_info(PREFIX_LOG, "  handleConnectionEvent() Read data sent from client");
                    // Read data sent from client
                    const ssize_t rlen = read(conn_data->conn_fd_, conn_data->buffer_.data() + conn_data->read_index_, conn_data->buffer_.size() - conn_data->read_index_ + 1);
                    concurrent_servers::log_info(PREFIX_LOG, "  rlen = ", rlen);
                    if (rlen > 0) {
                        conn_data->read_index_ += rlen;
                        concurrent_servers::log_info(PREFIX_LOG, "  received: ", conn_data->buffer_.data());
                        continue;
                    } else if (rlen == 0) {
                        concurrent_servers::log_info(PREFIX_LOG, "  end of file, fd=", conn_data->conn_fd_);
                        conn_data->ready_for_write_ = true;
                        break;
                    } else { // rlen < 0
                        if (errno == EWOULDBLOCK or errno == EAGAIN) {
                            // due to EPOLLONESHOT, after finishing reading all data in buffer,
                            // we need to rearm the client fd to catch its event again
                            concurrent_servers::log_info(PREFIX_LOG, "    rearm epoll event to write, fd=",
                                                         conn_data->conn_fd_);

                            event_.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;  // one shot edge triggered
                            event_.data.ptr = conn_data;

                            if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, conn_data->conn_fd_, &event_) == -1) {
                                concurrent_servers::log_error(PREFIX_LOG, "    epoll_ctl() failed to rearm");
                            }
                        } else {
                            concurrent_servers::log_error(PREFIX_LOG, "error on reading, fd=",
                                                          conn_data->conn_fd_, ", errno=",
                                                          errno, "\t", strerror(errno));
                        }
                        break;
                    }
                }
            } else if (conn_event.events & EPOLLOUT) {
                concurrent_servers::log_info(PREFIX_LOG, "  EPOLLOUT event, fd=", conn_data->conn_fd_);

                // Echo the data back to the client
                for (;;) {
                    const ssize_t wlen = write(conn_data->conn_fd_, conn_data->buffer_.data() + conn_data->write_index_, conn_data->read_index_ - conn_data->write_index_);
                    if (wlen >= 0) {
                        conn_data->write_index_ += wlen;
                    } else { // wlen < 0
                        if (errno == EWOULDBLOCK or errno == EAGAIN) {
                            // due to EPOLLONESHOT, after finishing writing all data in buffer,
                            // we need to rearm the client fd to catch its reading event again
                            concurrent_servers::log_info(PREFIX_LOG, "    rearm epoll event to read, fd=", conn_data->conn_fd_);
                            event_.events = EPOLLIN | EPOLLRDHUP | EPOLLET | EPOLLONESHOT;  // one shot edge triggered
                            event_.data.ptr = conn_data;
                            if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, conn_data->conn_fd_, &event_) == -1) {
                                concurrent_servers::log_error(PREFIX_LOG, "    epoll_ctl() failed to rearm");
//                                throw std::runtime_error(prefix_log_, "epoll_ctl() failed");
                            }
                        } else {
                            concurrent_servers::log_error(PREFIX_LOG, "    ERROR on writing, fd=", conn_data->conn_fd_, ", errno=", errno, "\t", strerror(errno));
                        }
                        break;
                    }
                }
            }
        }

        void closeConnection(int epoll_fd, int &conn_fd) {
            if (conn_fd < 0) {
                return;
            }

            data_manager_.remove(conn_fd);
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn_fd, nullptr);
            close(conn_fd);
            conn_fd = -1;
        }

        void log_client_info(const struct sockaddr_storage& cli_addr, const std::string &prefix_log) const {
            switch (cli_addr.ss_family) {
                case AF_INET:   /* IPv4 address. */ {
                    char addr_str[INET_ADDRSTRLEN];
                    auto *p = (struct sockaddr_in *) &cli_addr;
                    concurrent_servers::log_info(prefix_log, "New IPv4 client connected: address=", inet_ntop(p->sin_family, &p->sin_addr, addr_str, INET_ADDRSTRLEN), ", port=", p->sin_port);
                    break;
                }
                case AF_INET6:   /* IPv6 address. */ {
                    char addr_str[INET6_ADDRSTRLEN];
                    auto *p = (struct sockaddr_in6 *) &cli_addr;
                    concurrent_servers::log_info(prefix_log, "New IPv6 client connected: address=", inet_ntop(p->sin6_family, &p->sin6_addr, addr_str, INET_ADDRSTRLEN), ", port=", p->sin6_port);;
                    break;
                }
                default: {}
            }
        }
    };

    int server_sfd_;
    const std::string port_num_;
    const int backlog_;
    const int worker_num_;
    ConnectionDataManager data_manager_;

    int setupServerTcpSocket(const std::string& port_num, const int backlog, bool is_nonblock, bool reuse_port) {
        int server_sfd{};

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
            server_sfd = socket(rp->ai_family, socket_type, rp->ai_protocol);
            if (!server_sfd) {
                continue;
            }

            if (reuse_port) {
                int one = 1;
                setsockopt(server_sfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &one, sizeof(one));
            }

            if (bind(server_sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
                break;                  // Success
            }

            concurrent_servers::close_fd(server_sfd);
        }

        if (rp == nullptr) {               // No address succeeded
            throw std::runtime_error("Could not bind");
        }

        freeaddrinfo(result);           // No longer needed

        if (listen(server_sfd, backlog) < 0) {  // Listen to the socket
            concurrent_servers::close_fd(server_sfd);
            throw std::runtime_error("Could not listen to port " + port_num);
        }

        return server_sfd;
    }
};