#ifndef SCLOCK_H
#define SCLOCK_H

typedef unsigned long long microts;
typedef struct vhspec {
    microts initial_value; //in microseconds, e.g. 10^-6 of a second
    double drift_rate;
} vhspec;

long software_clock_gettime(vhspec *v);
int virtual_hardware_clock_gettime(vhspec *v);
int virtual_hardware_clock_init(vhspec *v);

#endif // SCLOCK_H
