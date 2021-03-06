#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include "sclock.h"

int main(int argc, char const *argv[]) {
    if (argc < 3) {
        printf("Usage: server [port] [master drift (PPM)]\n");
        exit(1);
    }

    vhspec server_clock;
    server_clock.drift_rate = atoi(argv[2]);
    if (virtual_hardware_clock_init(&server_clock) != 0) {
        printf("FATAL: Failed to initialize server virtual hardware clock.\n");
        exit(1);
    }

    int server_fd, new_socket;

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(atoi(argv[1]));

    int opt = 1;
    char buffer[MESSAGE_SIZE] = {0};

    // Create socket file descriptor. 0 indicates failure.
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) <= 0) {
        printf("FATAL: Socket creation failed.\n");
        exit(1);
    }

    // Allow reuse of local addresses and ports
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt))) {
        printf("FATAL: Socket option assignment failed.\n");
        exit(1);
    }

    // Bind the socket to a port
    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        printf("FATAL: Socket binding failed.\n");
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

        char return_buffer[MESSAGE_SIZE] = {0};

        /* Read the sequence number from the incoming buffer. */
        uint32_t sequence_number;
        sequence_number = ntohl(*(uint32_t *) buffer);
        printf("Received sequence number: %d\n", sequence_number);

        microts server_time;
        if (strncmp(QUERY_STRING, buffer + SEQ_NUM_SIZE, PAYLOAD_SIZE) == 0
            && virtual_hardware_clock_gettime(&server_clock, 
                                              &server_time) == 0) {

            /* Increment sequence number */
            *(uint32_t *) return_buffer = htonl(sequence_number + 1);

            /* Attach real_time value */
            *(uint64_t *) (return_buffer + SEQ_NUM_SIZE) = htonll(server_time);

            printf("Sending: [%d] [%ld]\n", sequence_number + 1, server_time);
            
            sendto(server_fd, return_buffer, MESSAGE_SIZE,
                   0, (struct sockaddr *) &client, slen);

        }
    }
}
