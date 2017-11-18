#include "sclock.h"
#include <time.h>

int real_hardware_clock_gettime(microts *result);

long software_clock_gettime(vhspec *v) {

}

/* Read the value of the virtual hardware clock.
   The VHC's value is computed as the real time + drift since initialization.
   Total drift = (time elapsed / 1*10^6) * PPM */
int virtual_hardware_clock_gettime(vhspec *v, microts *result) {
    microts real_time;
    if (real_hardware_clock_gettime(&real_time) != 0)
        return -1;

    microts elapsed_time = (real_time) - (v->initial_value);
    microts total_drift = (elapsed_time / MILLION) * (v->drift_rate);
    *result = elapsed_time + total_drift;
    return 0;
}

int virtual_hardware_clock_init(vhspec *v) {
    microts real_time;
    if (real_hardware_clock_gettime(&real_time) != 0)
        return -1;
    
    v->initial_value = real_time;
    return 0;
}

int real_hardware_clock_gettime(microts *result) {
    struct timespec monotonic;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &monotonic) != 0) {
        return -1;
    }
    
    unsigned long init_sec = monotonic.tv_sec * MILLION;
    unsigned long init_microsec = monotonic.tv_nsec / 1000;
    *result = init_sec + init_microsec;
    return 0;
}
