/*
 * Copyright (C) Pham Phi Long
 * Created on 3/12/19.
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
    strncpy(argv[0], "fork process per connection server", strlen(argv[0])); /* Change process name */
    tcpserver::log(argv[0]);           /* Print server process name */

    const std::string port_num = (argc >= 2) ? argv[1] : tcpserver::DEFAULT_PORT;
    const int backlog = (argc < 3) ? DEFAULT_BACKLOG : atoi(argv[2]);
    auto server_sfd = tcpserver::setup_server_tcp_socket(port_num, backlog);

    struct sockaddr_storage cli_addr{};
    char buffer[BUFF_SIZE];

    for (;;) {
        int cli_len = sizeof(cli_addr); /* Always reset this value */
        tcpserver::file_descriptor client_sfd{};

        if (!client_sfd.set_fd(accept(server_sfd.get_fd(), (struct sockaddr *) &cli_addr, (socklen_t * ) &cli_len))) {  /* Accept a new connection */
            fprintf(stderr, "Could not accept a new connection");
            server_sfd.close_fd();
            exit(EXIT_FAILURE);
        }

        tcpserver::log_client_info(cli_addr);

        /* Fork a new child process to handle the accepted connection */
        int child_pid;
        if ((child_pid = fork()) == -1) { /* failed to fork a child process */
            tcpserver::log("Could not fork a child process");
            client_sfd.close_fd();
            continue;
        } else if(child_pid > 0) { /* parent process */
            continue;
        } else if(child_pid == 0) { /* child process */
            strncpy(argv[0], "child process server", strlen(argv[0])); /* Change process name */
            for (;;) {
                // Read data sent from client
                const ssize_t rlen = read(client_sfd.get_fd(), buffer, sizeof(buffer));
                if (rlen < 0) {
                    fprintf(stderr, "ERROR on reading");
                    client_sfd.close_fd();
                    exit(EXIT_FAILURE);
                }

                if (rlen == 0) {
                    printf("  Connection closed\n");
                    client_sfd.close_fd();
                    exit(EXIT_SUCCESS);
                }

                printf("  received: %.*s\n", (int)rlen, buffer);

                // Echo the data back to the client
                const ssize_t wlen = write(client_sfd.get_fd(), buffer, (size_t)rlen);
                if (wlen < 0) {
                    fprintf(stderr, "ERROR on  writing");
                    client_sfd.close_fd();
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
}