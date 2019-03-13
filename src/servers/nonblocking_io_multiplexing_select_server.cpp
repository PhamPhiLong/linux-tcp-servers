/*
 * Copyright (C) Pham Phi Long
 * Created on 3/11/19.
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
#include <sys/select.h>

#include "constants.h"
#include "print_utility.h"
#include "file_descriptor.h"
#include "server_utility.h"

int main(int argc, char *argv[]) {
    tcpserver::log(argv[0]);           // Print server process name

    const std::string port_num = (argc >= 2) ? argv[1] : tcpserver::DEFAULT_PORT;
    const int backlog = (argc < 3) ? DEFAULT_BACKLOG : atoi(argv[2]);
    auto server_sfd = tcpserver::setup_server_tcp_socket(port_num, backlog, true);

    // Setup select()
    int max_sfd(server_sfd.get_fd()); /* highest-numbered file descriptor in any of the three sets */
    fd_set read_fd_backup_set, read_fd_set, write_fd_backup_set, write_fd_set; /* set of socket descriptors */
    FD_ZERO(&read_fd_backup_set);
    FD_SET(server_sfd.get_fd(), &read_fd_backup_set); /* add server listening socket fd to the select() read set*/
    FD_ZERO(&write_fd_backup_set);

    struct sockaddr_storage cli_addr{};
    char buffer[BUFF_SIZE];

    for (;;) {
        memcpy(&read_fd_set, &read_fd_backup_set, sizeof(read_fd_set));
        memcpy(&write_fd_set, &write_fd_backup_set, sizeof(write_fd_set));

        const int events = select(max_sfd + 1, &read_fd_set, nullptr, nullptr, nullptr);

        if (events == -1) {
            fprintf(stderr, "select() failed\n");
            continue;
        }

        int sfd_ready_no = events;
        for (int i=0; i <= max_sfd  &&  sfd_ready_no > 0; ++i) {
            if (FD_ISSET(i, &read_fd_set)) {
                sfd_ready_no -= 1; // reduce the number of unprocessed events in read set

                if (server_sfd.get_fd() == i) { // server received a new connection
                    tcpserver::file_descriptor client_sfd{};
                    for (;;) {
                        int cli_len = sizeof(cli_addr); // Always reset this value before calling accept()
                        if ((!client_sfd.set_fd(
                                accept4(server_sfd.get_fd(), (struct sockaddr *) &cli_addr, (socklen_t *) &cli_len, SOCK_NONBLOCK)))) {
                            if (errno != EWOULDBLOCK and errno != EAGAIN) {
                                fprintf(stderr, "Could not accept a new connection");
                                server_sfd.close_fd();
                                exit(EXIT_FAILURE);
                            } else {
                                break;
                            }
                        }

                        tcpserver::log_client_info(cli_addr);
                        tcpserver::log("Add new client socket fd ", client_sfd.get_fd());
                        FD_SET(client_sfd.get_fd(), &read_fd_backup_set);
                        max_sfd = std::max(max_sfd, client_sfd.get_fd());
                    }
                } else { // socket is readable
                    tcpserver::file_descriptor client_sfd{i};
                    bool connection_is_closed{false};

                    for (;;) {
                        // Read data sent from client
                        const ssize_t rlen = read(i, buffer, sizeof(buffer));
                        if (rlen < 0) {
                            if (errno != EWOULDBLOCK and errno != EAGAIN) {
                                fprintf(stderr, "ERROR on reading");
                                connection_is_closed = true;
                            }
                            break;
                        }

                        if (rlen == 0) {
                            printf("  Connection closed\n");
                            connection_is_closed = true;
                            break;
                        }

                        printf("  received: %.*s\n", (int)rlen, buffer);

                        // Echo the data back to the client
                        const ssize_t wlen = write(client_sfd.get_fd(), buffer, (size_t)rlen);
                        if (wlen < 0) {
                            if (errno != EWOULDBLOCK and errno != EAGAIN) {
                                fprintf(stderr, "ERROR on  writing");
                                connection_is_closed = true;
                            }
                            break;
                        }
                    }

                    if (connection_is_closed) {
                        client_sfd.close_fd();
                        FD_CLR(i, &read_fd_backup_set);
                        FD_CLR(i, &write_fd_backup_set);
                        if (i == max_sfd) {
                            // find the new max_sfd
                            for (max_sfd -= 1; max_sfd >= 0 and !FD_ISSET(max_sfd, &read_fd_backup_set); --max_sfd) {}
                            tcpserver::log("new max_sfd = ", max_sfd);
                        }
                    }
                }
            }
        }
    }
}