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
//#define CONNECTION_TIMEOUT 5000000
//#define PRINT_FREQUENCY 500000
//#define RAPPORT_PERIOD 1000000
//#define AMORTIZATION_PERIOD 500000
#define SERVER_SYNC_ATTEMPTS 50

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

    while (1) {
        /* Listen for incoming messages. Timeout assumes failure.
           If the incoming sequence number is from an old message, keep waiting. */
        int recv_len = recvfrom(socket, receive_buffer, MESSAGE_SIZE, 0, 0, 0);

        /* recv_len == -1 indicates timeout occurred. */
        if (recv_len < 0)
            break;

        /* Incorrect sequence numbers are equal to the sent sequence number
           or less than the sent sequence number. The two conditions are necessary
           for correct function with unsigned numbers. */
        if (recv_len != MESSAGE_SIZE ||
            ntohl(*(uint32_t *) receive_buffer) != sequence_number + 1)
            continue;

        /* Length and sequence number correct. Everything looks good. */
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

    microts sum_rtt = 0;
    microts worst_rtt = 0;

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

        sum_rtt += rtt;

        /* If best rtt, save request information */
        if (rtt < best_rtt) {
            best_rtt = rtt;
            best_request_local_time = request_local_time;
            best_received_server_value = server_value;
        }

        /* Keep the worst rtt too, for fun */
        if (rtt > worst_rtt)
            worst_rtt = rtt;

        if (i % 10 == 0 && i != 0)
            printf("\n");

        printf("[%d/%d] ", i + 1, SERVER_SYNC_ATTEMPTS + 1);
    }

    printf("\n");
    printf("Best Server Sync RTT: %ld, Worst Server Sync RTT: %ld\n",
           best_rtt, worst_rtt);

    printf("Average RTT: %lf\n", (double) sum_rtt / SERVER_SYNC_ATTEMPTS);

    /* Assuming RTT is evenly divided between server and client,
       current server time is the received timestamp plus
       (time elapsed since request) minus RTT/2. */
    microts current_time;
    virtual_hardware_clock_gettime(local, &current_time);

    microts est_server_time = best_received_server_value +
        (current_time - best_request_local_time) - (best_rtt / 2);

    local->offset = est_server_time - current_time;
    local->error = best_rtt / 2;

    printf("Est Server Time: %ld, Computed Server Time offset: %ld\n",
           est_server_time, local->offset);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 10) {
        printf("Usage: client [server IP] [server port]\n");
        printf("              [server simulated drift (PPM)] [client VH drift (PPM)]\n");
        printf("              [client-server RHW relative drift (PPM)]\n");
        printf("              [simulation runtime (seconds)]\n");
        printf("              [rapport period (usec)]\n");
        printf("              [timeout (usec)]\n");
        printf("              [amortization period (usec)]\n");
        printf("              [print frequency (usec)]\n");
        exit(1);
    }
    char *const SERVER_IP = argv[1];
    const int SERVER_PORT = atoi(argv[2]);

    /* Weigh server, local VHC drift by relative drift to correct native drift */
    const microts RELATIVE_DRIFT = atol(argv[5]);
    const microts SERVER_DRIFT = atol(argv[3]) + RELATIVE_DRIFT;
    const microts LOCAL_VHC_DRIFT = atol(argv[4]) + RELATIVE_DRIFT;

    const microts SIMULATION_RUNTIME = atoi(argv[6]) * MILLION;
    const microts RAPPORT_PERIOD = atol(argv[7]);
    const microts NETWORK_TIMEOUT = atol(argv[8]);
    const microts AMORTIZATION_PERIOD = atol(argv[9]);
    const microts PRINT_FREQUENCY = atol(argv[10]);

    vhspec local_hardware_clock = {0};
    local_hardware_clock.drift_rate = LOCAL_VHC_DRIFT;
    if (virtual_hardware_clock_init(&local_hardware_clock) != 0) {
        printf("FATAL: Failed to initialize local hardware clock.\n");
        exit(1);
    }

    scspec soft_clock = {0};
    memset(&soft_clock, 0, sizeof(soft_clock));
    soft_clock.amortization_period = AMORTIZATION_PERIOD;
    soft_clock.vhclock = &local_hardware_clock;

    /* The client's version of the server clock.
       Used for getting offline error measurements.
       The software clock is unaware of this clock (or else the simulation would
       be unnecessary). */
    vhspec server_clock = {0};
    server_clock.drift_rate = SERVER_DRIFT;
    if (virtual_hardware_clock_init(&server_clock) != 0) {
        printf("FATAL: Failed to initialize estimated server clock.\n");
        exit(1);
    }

    /* Create the client socket for sending/receiving data to the server. */
    int client_fd;
    if (create_client_socket(&client_fd) < 0
        || set_socket_timeout(client_fd, NETWORK_TIMEOUT)) {
        printf("FATAL: Could not create client socket.\n");
        exit(1);
    }

    /* Build the server address struct with IP, port */
    struct sockaddr_in server_addr = {0};
    uint32_t server_addr_len = sizeof(server_addr);
    if (build_server_address(&server_addr, SERVER_IP, SERVER_PORT)) {
        printf("FATAL: Could not construct server address.\n");
        exit(1);
    }

    /* Synchronize the local estimated server clock with the server clock */
    microts server_clock_value;
    virtual_hardware_clock_gettime(&server_clock, &server_clock_value);
    printf("Server clock before sync: %ld\n", server_clock_value);
    sync_server_clock(&server_clock, client_fd, &server_addr);
    virtual_hardware_clock_gettime(&server_clock, &server_clock_value);
    printf("Server clock after sync: %ld\n", server_clock_value);

    /* Use the real time clock to create data points at time intervals.
       Both print and rapport happen immediately. */
    microts last_rapport = 0;
    microts last_print = 0;

    microts current_real_time, local_server_time, local_hardware_clock_time,
        soft_clock_time, remote_est_time, error, e, simulation_end_time;

    real_hardware_clock_gettime(&current_real_time);
    simulation_end_time = current_real_time + SIMULATION_RUNTIME;

    printf("\n====== SIMULATION METADATA     =====\n");
    printf("Server IP: %s, Port: %d\n", SERVER_IP, SERVER_PORT);
    printf("Server Drift: %ld PPM, Client VHC Drift: %ld PPM\n",
           SERVER_DRIFT - RELATIVE_DRIFT, LOCAL_VHC_DRIFT - RELATIVE_DRIFT);
    printf("Relative Drift Weight: %ld\n", RELATIVE_DRIFT);
    printf("Local Server Time Error: %ld\n", server_clock.error);
    printf("Simulation runtime: %ld seconds\n", SIMULATION_RUNTIME / MILLION);
    printf("====== SIMULATION OUTPUT START =====\n");
    printf("Current Real Time,Local Server Time,Hardware Clock Time,\
Software Clock Time,Error,Remote Est Time,\n");

    /* Simulation begins, exits when time limit reached */
    while (current_real_time < simulation_end_time) {
        e = real_hardware_clock_gettime(&current_real_time)
            | virtual_hardware_clock_gettime(&server_clock, &local_server_time)
            | virtual_hardware_clock_gettime(soft_clock.vhclock,
                                             &local_hardware_clock_time)
            | software_clock_gettime(&soft_clock, &soft_clock_time);

        if (e != 0) {
            printf("FATAL: A clock read error occurred during runtime.\n");
            exit(1);
        }

        error = soft_clock_time - local_server_time;
/* Current Real Time,Local Server Time,Hardware Clock Time, */
/* Software Clock Time,Error,Remote Est Time, */

        if (current_real_time - last_print > PRINT_FREQUENCY) {
            printf("%ld,%ld,%ld,%ld,%ld,,\n",
                   current_real_time, local_server_time, local_hardware_clock_time,
                   soft_clock_time, error);

            last_print = current_real_time;
        }

        if (current_real_time - last_rapport > RAPPORT_PERIOD) {
            //printf("Attempting rapport...\t");

            /* for now, pretend min = server_clock.error */
            /* microts min = server_clock.error;*/

            /* If this code is reached, adjustment should have ended already
               and the software clock will reflect the hardware clock with no
               adjustments. Thus software_clock_gettime could be replaced with
               virtual_hardware_clock_gettime with insignificant differences. */
            microts request_local_time, response_local_time, response_value,
                response_local_hardware_time;
            software_clock_gettime(&soft_clock, &request_local_time);

            if (read_server_clock(&response_value, client_fd, &server_addr) != 0) {
                continue;
            }

            software_clock_gettime(&soft_clock, &response_local_time);
            virtual_hardware_clock_gettime(soft_clock.vhclock,
                                           &response_local_hardware_time);

            /* server time is in the interval [T + min, T + 2D - min]
               best estimate (middle of interval) is T + D */
            microts rtt = response_local_time - request_local_time;
            microts est_server_time = response_value + rtt/2;

            e = real_hardware_clock_gettime(&current_real_time)
                | virtual_hardware_clock_gettime(&server_clock, &local_server_time)
                | virtual_hardware_clock_gettime(soft_clock.vhclock,
                                                 &local_hardware_clock_time)
                | software_clock_gettime(&soft_clock, &soft_clock_time);

            if (e != 0) {
                printf("FATAL: A clock read error occurred during runtime.\n");
                exit(1);
            }
            error = soft_clock_time - local_server_time;
            printf("%ld,%ld,%ld,%ld,%ld,%ld,\n",
                   current_real_time, local_server_time, local_hardware_clock_time,
                   soft_clock_time, error, est_server_time);

            soft_clock.rapport_master = est_server_time;
            soft_clock.rapport_local = response_local_time;
            soft_clock.rapport_vhc = response_local_hardware_time;

            real_hardware_clock_gettime(&last_rapport);
            /* printf("Timestamp received: %ld\tRTT: %ld\tEst ST: %ld\n", */
            /* response_value, rtt, est_server_time); */
        }
    }
}
