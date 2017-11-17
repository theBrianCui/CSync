#include "sclock.h"
#include <time.h>

int real_hardware_clock_gettime(unsigned long long *result);

long software_clock_gettime(vhspec *v) {

}

long virtual_hardware_clock_gettime(vhspec *v) {

}

int virtual_hardware_clock_init(vhspec *v) {
    unsigned long long real_time;
    if (real_hardware_clock_gettime(&real_time) != 0) {
        return -1;
    }
    
    v->initial_value = real_time;
    return 0;
}

int real_hardware_clock_gettime(unsigned long long *result) {
    struct timespec monotonic;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &monotonic) != 0) {
        return -1;
    }
    
    unsigned long init_sec = monotonic.tv_sec * 1000000;
    unsigned long init_microsec = monotonic.tv_nsec / 1000;
    *result = init_sec + init_microsec;
    return 0;
}
