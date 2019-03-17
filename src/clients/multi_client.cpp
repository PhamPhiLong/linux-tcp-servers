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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <memory.h>
#include <unistd.h>
#include <thread>
#include <vector>

#include "constants.h"
#include "print_utility.h"

static const int num_threads = 10;

void send_ping_pong_message(const int portno, const struct hostent * const server);

int main(int argc, char *argv[]) {
    std::vector<std::thread> threads{};
    const int portno = (argc < 3) ? atoi(concurrent_servers::DEFAULT_PORT) : atoi(argv[2]);
    struct hostent *server = (argc < 2) ? gethostbyname("localhost") : gethostbyname(argv[1]);

    for (int i{0}; i<=num_threads; ++i) {
        threads.emplace_back(send_ping_pong_message, portno, server);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    return 0;
}

void send_ping_pong_message(const int portno, const struct hostent * const server) {
    for (int i{1}; i<=4; ++i) {
        const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("ERROR opening socket");
            exit(EXIT_FAILURE);
        }

        if (server == nullptr) {
            std::cerr << "ERROR, no such host" << std::endl;
            exit(EXIT_SUCCESS);
        }

        struct sockaddr_in serv_addr{};
        memset((char *) &serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        bcopy((char *) server->h_addr,
              (char *) &serv_addr.sin_addr.s_addr,
              server->h_length);
        serv_addr.sin_port = htons(portno);
        if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
            perror("ERROR connecting");
            exit(EXIT_FAILURE);
        }

        char buffer[BUFF_SIZE];
        memset(buffer, 0, BUFF_SIZE);
        const std::string msg{"ping pong " + std::to_string(i)};

        for (int j{1}; j <= 10; ++j) {
            if (write(sockfd, msg.data(), msg.length()) < 0) {
                perror("ERROR writing to socket");
                exit(EXIT_FAILURE);
            }
        }

        close(sockfd);
    }
}