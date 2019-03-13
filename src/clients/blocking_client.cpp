/*
 * Copyright (C) Pham Phi Long
 * Created on 3/11/19.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <memory.h>
#include <unistd.h>

#include "constants.h"
#include "print_utility.h"

int main(int argc, char *argv[]) {
    const int portno = (argc < 3) ? atoi(tcpserver::DEFAULT_PORT) : atoi(argv[2]);
    const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(EXIT_FAILURE);
    }

    struct hostent *server = (argc < 2) ? gethostbyname("localhost") : gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
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

    tcpserver::log("Please enter the message: ");
    char buffer[BUFF_SIZE];
    memset(buffer, 0, BUFF_SIZE);
    fgets(buffer, BUFF_SIZE, stdin);

    if (write(sockfd,buffer,strlen(buffer)) < 0) {
        perror("ERROR writing to socket");
        exit(EXIT_FAILURE);
    }

    memset(buffer, 0, BUFF_SIZE);
    if (read(sockfd, buffer, BUFF_SIZE) < 0) {
        perror("ERROR reading from socket");
        exit(EXIT_FAILURE);
    }
    printf("%s\n",buffer);
    return 0;
}