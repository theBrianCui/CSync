#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "sclock.h"
#define MESSAGE_SIZE 8

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

    int sockfd, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    char buffer[MESSAGE_SIZE];

    char *hostname = argv[1];
    int port = atoi(argv[2]);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Socket creation failed. Exiting.\n");
        exit(1);
    }

    struct timeval timeout;
    timeout.tv_sec = POLL_PERIOD / MILLION;
    timeout.tv_sec = POLL_PERIOD % MILLION;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
                   &timeout, sizeof(timeout)) < 0) {
        printf("Socket option assignment failed. Exiting.\n");
    }
    
    serveraddr.sin_family = AF_INET;
    inet_pton(AF_INET, hostname, &serveraddr.sin_addr);
    serveraddr.sin_port = htons(port);
    serverlen = sizeof(serveraddr);

    printf("Starting up...\n");
    while (1) {
        printf("Waiting for data...\n");
        int recv_len = recvfrom(sockfd, buffer, MESSAGE_SIZE, 0,
                                (struct sockaddr *) &serveraddr, &serverlen);

        printf("Not waiting anymore.\n");
        if (recv_len == MESSAGE_SIZE) {
            printf("Data received! %lu\n", *((uint64_t *) buffer));
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
