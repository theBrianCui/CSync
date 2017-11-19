#include "sclock.h"
#include <time.h>
#include <math.h>

int software_clock_gettime(vhspec *v, microts *result) {
    microts vhc_time;
    if (virtual_hardware_clock_gettime(v, &vhc_time) != 0)
        return -1;

    *result = vhc_time;
    return 0;
}

/* Read the value of the virtual hardware clock.
   The VHC's value is computed as the real time + drift since initialization.
   Total drift = (time elapsed / 1*10^6) * PPM */
int virtual_hardware_clock_gettime(vhspec *v, microts *result) {
    microts real_time;
    if (real_hardware_clock_gettime(&real_time) != 0)
        return -1;

    microts elapsed_time = (real_time) - (v->initial_value);

    /* compute whole_drift as drift expressed in whole numbers */
    microts whole_drift = (elapsed_time / MILLION) * (v->drift_rate);

    /* partial_drift is drift that occurs while a second has not fully elapsed.
       To preserve VHC continuity, we have to account for drift that has occured
       during the partial second that has elapsed. */
    double fraction_of_second = ((double) (elapsed_time % MILLION)) / MILLION;
    microts partial_drift = llrint(fraction_of_second * v->drift_rate);

    *result = elapsed_time + (whole_drift + partial_drift);
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
