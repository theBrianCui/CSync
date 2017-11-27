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
#define CONNECTION_TIMEOUT 5000000
#define PRINT_FREQUENCY 500000
#define RAPPORT_PERIOD 10000000
#define SERVER_SYNC_ATTEMPTS 10

uint32_t next_sequence_number() {
    static uint32_t current_sequence_number = 0;
    return current_sequence_number++;
}

int create_client_socket(int *fd) {
    /* Create a socket for sending / receiving requests to the server */
    int client_fd;
    if ((client_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Client socket creation failed.\n");
        return -1;
    }

    *fd = client_fd;
    return 0;
}

int set_socket_timeout(int fd, microts timeout_usec) {
    struct timeval timeout;
    timeout.tv_sec = timeout_usec / MILLION;
    timeout.tv_usec = timeout_usec % MILLION;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
                   &timeout, sizeof(timeout)) < 0) {
        printf("Client timeout assignment failed.\n");
        return -1;
    }
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

/* Attempt to read the remote clock.
   If successful, stores the timestamp read in result and returns 0.
   If unsuccessful, returns -1. */
int read_server_clock(microts *result, int socket, struct sockaddr_in *server_addr) {
    /* Construct message: [seq number] [query string] */
    char request_buffer[MESSAGE_SIZE] = {0};
    char receive_buffer[MESSAGE_SIZE] = {0};

    /* Assign and increment sequence number */
    uint32_t sequence_number = next_sequence_number();
    *(uint32_t *) request_buffer = htonl(sequence_number);

    /* Insert query string */
    strncpy(request_buffer + SEQ_NUM_SIZE, QUERY_STRING, PAYLOAD_SIZE);

    /* Send the request_buffer to the server */
    int sresult = sendto(socket, request_buffer, MESSAGE_SIZE, MSG_DONTWAIT,
                         (struct sockaddr *) server_addr, sizeof(*server_addr));

    if (sresult == -1) {
        printf("FATAL: sendto failed in read_server_clock. %s\n", strerror(errno));
        exit(1);
    }

    /* Listen for incoming messages. Timeout assumes failure. */
    int recv_len = recvfrom(socket, receive_buffer, MESSAGE_SIZE, 0, 0, 0);

    /* Correct message includes sequence_number + 1 */
    if (recv_len == MESSAGE_SIZE
        && ntohl(*(uint32_t *) receive_buffer) == sequence_number + 1) {
        *result = ntohll(*(uint64_t *) (receive_buffer + SEQ_NUM_SIZE));
        return 0;
    }

    /* Message was lost or timed out. */
    printf("WARN: A server response timed out.\n");
    return -1;
}

int sync_server_clock(vhspec *local, int socket,
                      struct sockaddr_in *server_addr) {

    /* Synchronize the server_clock with the actual server_clock
       as best as possible by assigning an offset. */
    printf("Synchronizing server_clock...\n");

    microts best_rtt = LLONG_MAX;
    microts best_request_local_time;
    microts best_received_server_value;
    for (int i = 0; i < SERVER_SYNC_ATTEMPTS; ++i) {
        /* Save the current time to request_time[i] */
        microts request_local_time;
        virtual_hardware_clock_gettime(local, &request_local_time);

        /* Read the remote clock */
        microts server_value;
        if (read_server_clock(&server_value, socket, server_addr) < 0) {
            /* Assume message was lost. Retry. */
            --i;
            continue;
        }

        /* Read success. Compute the RTT */
        microts response_local_time;
        virtual_hardware_clock_gettime(local, &response_local_time);
        microts rtt = response_local_time - request_local_time;

        if (rtt < best_rtt) {
            best_rtt = rtt;
            best_request_local_time = request_local_time;
            best_received_server_value = server_value;
        }

        printf("[%d/%d] ", i + 1, SERVER_SYNC_ATTEMPTS + 1);
    }

    printf("\n");
    printf("Minimum Recorded Server Sync RTT: %ld\n", best_rtt);

    /* Assuming RTT is evenly divided between server and client,
       current server time is the received timestamp plus
       (time elapsed since request) plus RTT/2. */
    microts current_time;
    virtual_hardware_clock_gettime(local, &current_time);

    microts est_server_time = best_received_server_value +
        (current_time - best_request_local_time) + (best_rtt / 2);

    local->offset = est_server_time - current_time;
    local->error = best_rtt / 2;

    printf("Est Server Time: %ld, Computed Server Time offset: %ld\n",
           est_server_time, local->offset);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 5) {
        printf("Usage: client [server IP] [server port]\n");
        printf("              [server drift (PPM)] [client drift (PPM)]\n");
        exit(1);
    }

    vhspec local_hardware_clock = {0};
    local_hardware_clock.drift_rate = atoi(argv[4]);
    if (virtual_hardware_clock_init(&local_hardware_clock) != 0) {
        printf("FATAL: Failed to initialize local hardware clock.\n");
        exit(1);
    }

    scspec soft_clock = {0};
    memset(&soft_clock, 0, sizeof(soft_clock));
    soft_clock.amortization_period = 4000000; // four seconds
    soft_clock.vhclock = &local_hardware_clock;

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

    /* Create the client socket for sending/receiving data to the server. */
    int client_fd;
    if (create_client_socket(&client_fd) < 0
        || set_socket_timeout(client_fd, CONNECTION_TIMEOUT)) {
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
    microts server_clock_value;
    virtual_hardware_clock_gettime(&server_clock, &server_clock_value);
    printf("Server clock before sync: %ld\n", server_clock_value);
    sync_server_clock(&server_clock, client_fd, &server_addr);
    virtual_hardware_clock_gettime(&server_clock, &server_clock_value);
    printf("Server clock after sync: %ld\n", server_clock_value);

    /* Use the real time clock to create data points at time intervals */
    microts last_rapport;
    microts last_print;
    if (real_hardware_clock_gettime(&last_rapport) != 0) {
        printf("An error occurred when reading the real time.\n");
        exit(1);
    }
    last_print = last_rapport;

    while (1) {
        microts current_real_time, est_server_time, local_hardware_clock_time,
            soft_clock_time, error;
        int e = real_hardware_clock_gettime(&current_real_time)
            | virtual_hardware_clock_gettime(&server_clock, &est_server_time)
            | virtual_hardware_clock_gettime(soft_clock.vhclock,
                                             &local_hardware_clock_time)
            | software_clock_gettime(&soft_clock, &soft_clock_time);

        if (e != 0) {
            printf("An error occurred during runtime.\n");
            exit(1);
        }

        error = soft_clock_time - est_server_time;
        if (current_real_time - last_print > PRINT_FREQUENCY) {
            printf("RT: %ld\tST: %ld\tLT: %ld\te: %ld +- %ld\n",
                   current_real_time, est_server_time, soft_clock_time,
                   error, server_clock.error);

            last_print = current_real_time;
        }

        if (current_real_time - last_rapport > RAPPORT_PERIOD) {
            printf("Performing rapport.\n");
            soft_clock.rapport_master = est_server_time;
            soft_clock.rapport_local = soft_clock_time;
            soft_clock.rapport_vhc = local_hardware_clock_time;
            last_rapport = current_real_time;
        }
    }
}
