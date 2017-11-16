#ifndef SCLOCK_H
#define SCLOCK_H

typedef struct vhspec {
    long initial_value;
    double drift_rate;
} vhspec;

long software_clock_gettime(vhspec *v);
long virtual_hardware_clock_gettime(vhspec *v);
long virtual_hardware_clock_init(vhspec *v);

#endif // SCLOCK_H
