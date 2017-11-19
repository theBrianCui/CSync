#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "sclock.h"

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

    while (1) {
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
        if (current_real_time - last_print > 500000) {
            printf("RT: %lld\tDCT: %lld\tFCT: %lld\tSCT: %lld\tE: %lld\n",
                   current_real_time, doubling_clock_time,
                   fast_clock_time, soft_clock_time, error);
            last_print = current_real_time;
        }

        if (current_real_time - last_rapport > 10000000) {
            printf("Performing rapport.\n");
            soft_clock.rapport_master = doubling_clock_time;
            soft_clock.rapport_local = soft_clock_time;
            soft_clock.rapport_vhc = fast_clock_time;
            last_rapport = current_real_time;
        }
    }
}
