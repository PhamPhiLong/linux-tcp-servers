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
#include <sys/epoll.h>
#include <sys/wait.h>

#include "constants.h"
#include "print_utility.h"
#include "file_descriptor.h"
#include "server_utility.h"


int main(int argc, char *argv[]) {
    const std::string port_num = (argc >= 2) ? argv[1] : concurrent_servers::DEFAULT_PORT;
    const int backlog = (argc >= 3) ? atoi(argv[2]) : DEFAULT_BACKLOG;
    const int worker_process_num = (argc >= 4) ? atoi(argv[3]) : concurrent_servers::DEFAULT_WORKER_PROCESS_NUMBER;
    concurrent_servers::file_descriptor server_sfd;

    const size_t process_name_len{strlen(argv[0])};
    strncpy(argv[0], "tcp-server master process", process_name_len); /* Change process name */
    concurrent_servers::log_info(argv[0]);           // Print process name
    strncpy(argv[0], "tcp-server worker process pid=", process_name_len); /* Change process name */

    // Pre-fork worker processes to distribute accept() and read()
    // for accept(), the server listening socket fd
    for (int i{0}; i < worker_process_num; ++ i) {
        int child_pid;

        if ((child_pid = fork()) == -1) { // failed to fork a child process
            throw std::runtime_error("Failed to fork worker processes");
        } else if (child_pid > 0) { // parent process
            std::cout << "\033[32m" << "Forked new worker process with pid=" << child_pid << "\033[0m" << std::endl;
        } else if (child_pid == 0) { // child process
            try {
                server_sfd = concurrent_servers::setup_server_tcp_socket(port_num, backlog, true, true);

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