#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include "sclock.h"
#define MESSAGE_SIZE 8

int main(int argc, char const *argv[]) {
    int server_fd, new_socket;

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    int opt = 1;
    char buffer[MESSAGE_SIZE] = {0};
    const char *query_string = "time = ?";

    // Create socket file descriptor. 0 indicates failure.
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) <= 0) {
        printf("Socket creation failed. Exiting.\n");
        exit( 1);
    }

    // Allow reuse of local addresses and ports
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt))) {
        printf("Socket option assignment failed. Exiting.\n");
        exit(1);
    }

    // Bind the socket to a port
    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        printf("Socket binding failed. Exiting.\n");
        exit(1);
    }

    struct sockaddr_in client;
    socklen_t slen = sizeof(client);
    while (1) {
        memset(buffer, 0, MESSAGE_SIZE);
        printf("Waiting for data...\n");
        fflush(stdout);

        /* recvfrom is a blocking call. It stores sender information in `client`
           and the received datagram in `buffer` with size `recv_len`. */
        int recv_len;
        if ((recv_len = recvfrom(server_fd, buffer, MESSAGE_SIZE, 0,
                                 (struct sockaddr *) &client, &slen)) < 0) {
            printf("recvfrom failed. Exiting.\n");
            exit(1);
        }

        printf("Packet received of length %d.\n", recv_len);
        printf("Received packet from %s:%d\n", inet_ntoa(client.sin_addr),
               ntohs(client.sin_port));
        printf("Data: %s, strlen(): %zu\n" , buffer, strlen(buffer));

        microts real_time;
        if (strncmp(query_string, buffer, MESSAGE_SIZE) == 0
            && real_hardware_clock_gettime(&real_time)) {
            
            sendto(server_fd, &real_time, MESSAGE_SIZE,
                   0, (struct sockaddr *) &client, slen);
        }
    }
}
