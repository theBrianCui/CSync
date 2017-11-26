#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h>
#include "sclock.h"

/* Unless otherwise specified, constants are given in microseconds
   (e.g. 1 * 10^6 microseconds = 1000000 = 1 second ) */
#define POLL_PERIOD 2000000
#define RAPPORT_PERIOD 10000000
#define SERVER_SYNC_ATTEMPTS 10

uint32_t next_sequence_number() {
    static uint32_t current_sequence_number = 0;
    return current_sequence_number++;
}

int create_client_socket(int *fd, microts timeout_usec) {
    /* Create a socket for sending / receiving requests to the server */
    int client_fd;
    if ((client_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Client socket creation failed.\n");
        return -1;
    }

    /* Assign a receive timeout as a function of the POLL_PERIOD */
    struct timeval timeout;
    timeout.tv_sec = timeout_usec / MILLION;
    timeout.tv_usec = timeout_usec % MILLION;
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO,
                   &timeout, sizeof(timeout)) < 0) {
        printf("Client timeout assignment failed.\n");
        return -1;
    }

    *fd = client_fd;
    return 0;
}

int build_server_address(struct sockaddr_in *server_addr, char *ip, int port) {
    server_addr->sin_family = AF_INET;
    if (inet_pton(AF_INET, ip, &server_addr->sin_addr) != 1) {
        printf("Invalid server IP address.\n");
        return -1;
    }
    server_addr->sin_port = htons(port);
    return 0;
}

int sync_server_clock(vhspec *local, int socket, 
                      struct sockaddr_in *server_addr) {
    
    /* Synchronize the server_clock with the actual server_clock
       as best as possible by assigning an offset. */
    printf("Synchronizing server_clock...\n");

    microts request_local_time[SERVER_SYNC_ATTEMPTS] = {0};
    microts received_server_time[SERVER_SYNC_ATTEMPTS] = {0};
    microts rtt[SERVER_SYNC_ATTEMPTS] = {0};
    uint32_t server_addr_len = sizeof(*server_addr);

    for (int i = 0; i < SERVER_SYNC_ATTEMPTS; ++i) {
        /* Construct message: [seq number] [query string] */
        char request_buffer[MESSAGE_SIZE] = {0};
        char receive_buffer[MESSAGE_SIZE] = {0};

        /* Assign and increment sequence number */
        uint32_t sequence_number = next_sequence_number();
        *(uint32_t *) request_buffer = htonl(sequence_number);

        /* Insert query string */
        strncpy(request_buffer + SEQ_NUM_SIZE, QUERY_STRING, PAYLOAD_SIZE);

        /* Save the current time to request_time[i] */
        virtual_hardware_clock_gettime(local, request_local_time + i);

        /* Send the request_buffer to the server */
        int sresult = sendto(socket, request_buffer, MESSAGE_SIZE, MSG_DONTWAIT,
               (struct sockaddr *) server_addr, server_addr_len);

        if (sresult == -1) {
            printf("Server Clock Sync Error: %s\n", strerror(errno));
            exit(1);
        }

        /* Listen for incoming messages. Timeout assumes failure. */
        int recv_len = recvfrom(socket, receive_buffer, MESSAGE_SIZE, 0, 0, 0);

        /* Correct message includes sequence_number + 1 */
        if (recv_len == MESSAGE_SIZE
            && ntohl(*(uint32_t *) receive_buffer) == sequence_number + 1) {

            /* Compute local estimated RTT */
            microts received_local_time;
            virtual_hardware_clock_gettime(local, &received_local_time);
            rtt[i] = received_local_time - request_local_time[i];

            microts received_ts =
                ntohll(*(uint64_t *) (receive_buffer + SEQ_NUM_SIZE));

            received_server_time[i] = received_ts;

            printf("[%d/%d] ", i + 1, SERVER_SYNC_ATTEMPTS);
            continue;
        }

        /* Assume message was lost. Retry with a new sequence number. */
        --i;
        continue;
    }
    printf("\n");

    microts min_rtt = LLONG_MAX;
    int min_rtt_index = -1;
    for (int i = 0; i < SERVER_SYNC_ATTEMPTS; ++i) {
        if (rtt[i] < min_rtt) {
            min_rtt = rtt[i];
            min_rtt_index = i;
        }
    }

    printf("Minimum Recorded Server Sync RTT: %ld\n", min_rtt);
    /* Assuming RTT is evenly divided between server and client,
       current server time is the received timestamp plus 
       time elapsed since (time the request was made plus RTT/2). */
    microts current_time;
    virtual_hardware_clock_gettime(local, &current_time);
    microts est_server_time = received_server_time[min_rtt_index] +
        (current_time - 
         (request_local_time[min_rtt_index] + rtt[min_rtt_index] / 2));

    local->offset = est_server_time - current_time;

    printf("Est Server Time: %ld, Computed Server Time offset: %ld\n",
           est_server_time, local->offset);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 4) {
        printf("Usage: client [server IP] [server port] [server drift (PPM)]\n");
        exit(1);
    }

    vhspec doubling_clock = {0};
    doubling_clock.drift_rate = 1000000;
    if (virtual_hardware_clock_init(&doubling_clock) != 0) {
        printf("An error occurred during initialization of doubling_clock.\n");
        exit(1);
    }

    vhspec fast_clock = {0};
    fast_clock.drift_rate = 500000;
    if (virtual_hardware_clock_init(&fast_clock) != 0) {
        printf("An error occurred during initialiation of fast_clock.\n");
        exit(1);
    }

    scspec soft_clock = {0};
    memset(&soft_clock, 0, sizeof(soft_clock));
    soft_clock.amortization_period = 4000000; // four seconds
    soft_clock.vhclock = &fast_clock;

    /* The client's version of the server clock.
       Used for getting offline error measurements.
       The software clock is unaware of this clock (or else the simulation would
       be unnecessary). */
    vhspec server_clock = {0};
    server_clock.drift_rate = atoi(argv[3]);
    if (virtual_hardware_clock_init(&server_clock) != 0) {
        printf("An error occurred during initialization of server_clock.\n");
        exit(1);
    }

    microts last_rapport;
    microts last_print;
    if (real_hardware_clock_gettime(&last_rapport) != 0) {
        printf("An error occurred when reading the real time.\n");
        exit(1);
    }
    last_print = last_rapport;

    /* Create the client socket for sending/receiving data to the server. */
    int client_fd;
    if (create_client_socket(&client_fd, POLL_PERIOD) < 0) {
        printf("Could not create client socket.\n");
        exit(1);
    }

    /* Build the server address struct with IP, port */
    struct sockaddr_in server_addr = {0};
    uint32_t server_addr_len = sizeof(server_addr);
    if (build_server_address(&server_addr, argv[1], atoi(argv[2]))) {
        printf("Could not construct server address.\n");
        exit(1);
    }

    /* Synchronize the local estimated server clock with the server clock */
    sync_server_clock(&server_clock, client_fd, &server_addr);

    while (1) {
        /* Construct message: [seq number] [query string] */
        char request_buffer[MESSAGE_SIZE] = {0};
        char receive_buffer[MESSAGE_SIZE] = {0};

        /* Assign and increment sequence number */
        uint32_t sequence_number = next_sequence_number();
        *(uint32_t *) request_buffer = htonl(sequence_number);

        /* Insert query string */
        strncpy(request_buffer + SEQ_NUM_SIZE, QUERY_STRING, PAYLOAD_SIZE);

        /* Send the request_buffer to the server */
        sendto(client_fd, request_buffer, MESSAGE_SIZE, MSG_DONTWAIT,
               (struct sockaddr *) &server_addr, server_addr_len);

        /* Listen for incoming messages (will eventually timeout) */
        int recv_len = recvfrom(client_fd, receive_buffer, 
                                MESSAGE_SIZE, 0, 0, 0);

        if (recv_len == MESSAGE_SIZE) {
            uint32_t received_seq_num = ntohl(*(uint32_t *) receive_buffer);
            if (received_seq_num == sequence_number + 1) {
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
