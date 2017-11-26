#include "sclock.h"
#include <time.h>
#include <math.h>
#include <stdio.h>

int software_clock_gettime(scspec *s, microts *result) {
    microts vhc_time;
    if (virtual_hardware_clock_gettime(s->vhclock, &vhc_time) != 0)
        return -1;

    double multiplier;
    microts offset;

    /* If amortization has complete, offset is N = L' - H'
       where L' is the local time after amortization and
       H' is the hardware clock value after amortization.
       L' = M + a, H' = H + a */

    if (s->rapport_vhc + s->amortization_period <= vhc_time) {
        multiplier = 0.0;
        offset = (s->rapport_master + s->amortization_period) -
            (s->rapport_vhc + s->amortization_period);

    } else {
        printf("s->rapport_master: %ld, s->rapport_local: %ld, diff: %lf\n",
               s->rapport_master, s->rapport_local,
               (double) (s->rapport_master - s->rapport_local));
        // m = (M - L)/a
        multiplier = ((double) (s->rapport_master - s->rapport_local)) /
            ((double) s->amortization_period);

        // N = L - (1 + m) * H
        offset = llrint(s->rapport_local - ((1 + multiplier) * s->rapport_vhc));
    }

    printf("Multiplier: %lf, Offset: %ld\n", multiplier, offset);

    // L = H * (1 + m) + N
    *result = llrint(vhc_time * (1 + multiplier)) + offset;
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

    *result = elapsed_time + (whole_drift + partial_drift) + v->offset;
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
