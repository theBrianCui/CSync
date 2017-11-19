#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "sclock.h"

int main(int argc, char *argv[])
{
    vhspec doubling_clock;
    doubling_clock.drift_rate = 1000000;
    if (virtual_hardware_clock_init(&doubling_clock) != 0) {
        printf("An unknown error occurred during initialization.\n");
        exit(1);
    }

    while (1) {
        microts real_time;
        microts vhc_time;
        if (real_hardware_clock_gettime(&real_time)
            || software_clock_gettime(&doubling_clock, &vhc_time)) {
            printf("An unknown error occurred while reading the time.\n");
            exit(1);
        }
        
        printf("Real time:\t%lld\nVHC Time:\t%lld\n", real_time, vhc_time);
        sleep(1);
    }
}
