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

#include "constants.h"
#include "print_utility.h"
#include "file_descriptor.h"

int main(int argc, char *argv[]) {
    tcpserver::log(argv[0]);           /* Print server process name */
    bool enable_log = true;

    /* Setup server address */
    struct addrinfo hints{};
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP socket */
    hints.ai_flags = AI_PASSIVE;     /* For wildcard IP address */
    hints.ai_protocol = 0;           /* Any protocol */
    hints.ai_canonname = nullptr;
    hints.ai_addr = nullptr;
    hints.ai_next = nullptr;

    struct addrinfo *result, *rp;
    const char *portNo = (argc >= 2) ? argv[1] : tcpserver::DEFAULT_PORT;
    const int s = getaddrinfo(nullptr, portNo, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
     * Try each address until we successfully bind(2).
     * If socket(2) (or bind(2)) fails, we (close the socket
     *  and) try the next address.
     */
    tcpserver::file_descriptor server_sfd{};
    for (rp = result; rp != nullptr; rp = rp->ai_next) {
        if (!server_sfd.set_fd(socket(rp->ai_family, rp->ai_socktype | SOCK_NONBLOCK, rp->ai_protocol))) {
            continue;
        }

        if (bind(server_sfd.get_fd(), rp->ai_addr, rp->ai_addrlen) == 0) {
            break;                  /* Success */
        }

        server_sfd.close_fd();
    }

    if (rp == nullptr) {               /* No address succeeded */
        fprintf(stderr, "Could not bind\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result);           /* No longer needed */


    const auto backlog = (argc < 3) ? DEFAULT_BACKLOG : atoi(argv[2]);
    if (listen(server_sfd.get_fd(), backlog) < 0) {  /* Listen to the socket */
        fprintf(stderr, "Could not listen to port %s\n", portNo);
        server_sfd.close_fd();
        exit(EXIT_FAILURE);
    }

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

        if (enable_log) {
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

        for (;;) {
            // Read data sent from client
            const ssize_t rlen = read(client_sfd.get_fd(), buffer, sizeof(buffer));
            if (rlen < 0) {
                fprintf(stderr, "ERROR on reading");
                client_sfd.close_fd();
                break;
            }

            if (rlen == 0) {
                printf("  Connection closed\n");
                client_sfd.close_fd();
                break;
            }

            printf("  received: %.*s\n", (int)rlen, buffer);

            // Echo the data back to the client
            const ssize_t wlen = write(client_sfd.get_fd(), buffer, (size_t)rlen);
            if (wlen < 0) {
                fprintf(stderr, "ERROR on  writing");
                client_sfd.close_fd();
                break;
            }
        }
    }
}