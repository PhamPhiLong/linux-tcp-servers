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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <signal.h>

#include "constants.h"
#include "print_utility.h"
#include "file_descriptor.h"
#include "server_utility.h"

int main(int argc, char *argv[]) {
    concurrent_servers::log(argv[0]);           /* Print server process name */

    const std::string port_num = (argc >= 2) ? argv[1] : concurrent_servers::DEFAULT_PORT;
    const int backlog = (argc < 3) ? DEFAULT_BACKLOG : atoi(argv[2]);
    concurrent_servers::file_descriptor server_sfd{};

    try {
        server_sfd = concurrent_servers::setup_server_tcp_socket(port_num, backlog);
        struct sockaddr_storage cli_addr{};
        char buffer[BUFF_SIZE];

        for (;;) {
            int cli_len = sizeof(cli_addr); /* Always reset this value */
            concurrent_servers::file_descriptor client_sfd{};

            if (!client_sfd.set_fd(accept(server_sfd.get_fd(), (struct sockaddr *) &cli_addr, (socklen_t * ) &cli_len))) {  /* Accept a new connection */
                throw std::runtime_error("Could not accept a new connection");
            }

            concurrent_servers::log_client_info(cli_addr);

            for (;;) {
                // Read data sent from client
                const ssize_t rlen = read(client_sfd.get_fd(), buffer, sizeof(buffer));
                if (rlen < 0) {
                    client_sfd.close_fd();
                    throw std::runtime_error("ERROR on reading");
                }

                if (rlen == 0) {
                    std::cout << "  Connection closed" << std::endl;
                    client_sfd.close_fd();
                    break;
                }

                std::cout << "  received: " << buffer << std::endl;

                // Echo the data back to the client
                const ssize_t wlen = write(client_sfd.get_fd(), buffer, (size_t)rlen);
                if (wlen < 0) {
                    client_sfd.close_fd();
                    throw std::runtime_error("ERROR on  writing");
                }
            }
        }
    } catch (const std::runtime_error& e) {
        concurrent_servers::log_error(e.what(), "\n\t", strerror(errno));
        server_sfd.close_fd();
        exit(EXIT_FAILURE);
    }
}