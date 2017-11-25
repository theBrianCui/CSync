#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "sclock.h"

/* Unless otherwise specified, constants are given in microseconds
   (e.g. 1 * 10^6 microseconds = 1000000 = 1 second ) */
#define POLL_PERIOD 500000
#define RAPPORT_PERIOD 10000000

int main(int argc, char *argv[])
{
    vhspec doubling_clock;
    doubling_clock.drift_rate = 1000000;
    if (virtual_hardware_clock_init(&doubling_clock) != 0) {
        printf("An error occurred during initialization of doubling_clock.\n");
        exit(1);
    }

    vhspec fast_clock;
    fast_clock.drift_rate = 500000;
    if (virtual_hardware_clock_init(&fast_clock) != 0) {
        printf("An error occurred during initialiation of fast_clock.\n");
        exit(1);
    }

    scspec soft_clock;
    memset(&soft_clock, 0, sizeof(soft_clock));
    soft_clock.amortization_period = 4000000; // four seconds
    soft_clock.vhclock = &fast_clock;

    microts last_rapport;
    microts last_print;
    if (real_hardware_clock_gettime(&last_rapport) != 0) {
        printf("An error occurred when reading the real time.\n");
        exit(1);
    }
    last_print = last_rapport;

    /* Create a socket for sending / receiving requests to the server */
    int client_fd;
    if ((client_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Socket creation failed. Exiting.\n");
        exit(1);
    }

    /* Assign a receive timeout as a function of the POLL_PERIOD */
    struct timeval timeout;
    timeout.tv_sec = POLL_PERIOD / MILLION;
    timeout.tv_usec = POLL_PERIOD % MILLION;
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO,
                   &timeout, sizeof(timeout)) < 0) {
        printf("Socket option assignment failed. Exiting.\n");
    }

    /* Build the server address struct with IP, port */
    struct sockaddr_in server_addr;
    int server_addr_len = sizeof(server_addr);
    if (argc < 3) {
        printf("Usage: client [ip address] [port]\n");
        exit(1);
    }

    char *hostname = argv[1];
    int port = atoi(argv[2]);
    server_addr.sin_family = AF_INET;

    /* Convert IP address from text to binary form */
    inet_pton(AF_INET, hostname, &server_addr.sin_addr);
    server_addr.sin_port = htons(port);

    uint32_t next_seq_num = 0;
    printf("Starting up...\n");
    while (1) {
        /* Construct message: [seq number] [query string] */
        char request_buffer[MESSAGE_SIZE] = {0};
        char receive_buffer[MESSAGE_SIZE] = {0};

        /* Assign and increment sequence number */
        *(uint32_t *) request_buffer = htonl(next_seq_num++);

        /* Insert query string */
        strncpy(request_buffer + SEQ_NUM_SIZE, QUERY_STRING, PAYLOAD_SIZE);

        /* Send the request_buffer to the server */
        sendto(client_fd, request_buffer, MESSAGE_SIZE, MSG_DONTWAIT,
               (struct sockaddr *) &server_addr, server_addr_len);

        /* Listen for incoming messages (will eventually timeout) */
        int recv_len = recvfrom(client_fd, receive_buffer, MESSAGE_SIZE, 0,
                                (struct sockaddr *) &server_addr,
                                &server_addr_len);

        if (recv_len == MESSAGE_SIZE) {
            uint32_t received_seq_num = ntohl(*(uint32_t *) receive_buffer);
            if (received_seq_num == next_seq_num) {
                microts received_ts =
                    ntohll(*(uint64_t *) (receive_buffer + SEQ_NUM_SIZE));

                printf("Received: [%d] [%ld]\n", received_seq_num, received_ts);
            }
        }

        microts current_real_time, doubling_clock_time,
            fast_clock_time, soft_clock_time, error;

        int e = real_hardware_clock_gettime(&current_real_time)
            | virtual_hardware_clock_gettime(&doubling_clock, &doubling_clock_time)
            | virtual_hardware_clock_gettime(&fast_clock, &fast_clock_time)
            | software_clock_gettime(&soft_clock, &soft_clock_time);

        if (e != 0) {
            printf("An error occurred during runtime.\n");
            exit(1);
        }

        error = soft_clock_time - doubling_clock_time;
        printf("RT: %ld\tDCT: %ld\tFCT: %ld\tSCT: %ld\tE: %ld\n",
               current_real_time, doubling_clock_time,
               fast_clock_time, soft_clock_time, error);

        if (current_real_time - last_rapport > RAPPORT_PERIOD) {
            printf("Performing rapport.\n");
            soft_clock.rapport_master = doubling_clock_time;
            soft_clock.rapport_local = soft_clock_time;
            soft_clock.rapport_vhc = fast_clock_time;
            last_rapport = current_real_time;
        }
    }
}
