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
#include <vector>

#include "constants.h"
#include "print_utility.h"

int main(int argc, char *argv[]) {
    const int portno = (argc < 3) ? atoi(concurrent_servers::DEFAULT_PORT) : atoi(argv[2]);
    const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(EXIT_FAILURE);
    }

    struct hostent *server = (argc < 2) ? gethostbyname("localhost") : gethostbyname(argv[1]);
    if (server == NULL) {
        std::cerr << "ERROR, no such host" << std::endl;
        exit(EXIT_SUCCESS);
    }

    struct sockaddr_in serv_addr;
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
        perror("ERROR connecting");
        exit(EXIT_FAILURE);
    }

    std::string write_buffer = "Hello, I am the client.\nPlease echo back what I'm saying.";

    for (int i=0; i<10; i++) {
        if (write(sockfd,write_buffer.data(),write_buffer.size()) < 0) {
            perror("ERROR writing to socket");
            std::cout << "ERROR writing to socket" << std::endl;
            exit(EXIT_FAILURE);
        }

        std::vector<char> read_buffer(1024, 0);
        if (read(sockfd, read_buffer.data(), read_buffer.size()) < 0) {
            perror("ERROR reading from socket");
            exit(EXIT_FAILURE);
        }

        std::cout << read_buffer.data() << std::endl;
    }

    return 0;
}