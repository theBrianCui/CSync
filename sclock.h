#ifndef SCLOCK_H
#define SCLOCK_H

static const int MILLION = 1000000;

/* Timestamp supporting microsecond precision.
   1 microts = 1 microsecond. */
typedef unsigned long long microts;

/* Specification for a hardware clock.
   Initialize with a drift_rate and then calling
   virtual_hardware_clock_init to assign the initial_value. */
typedef struct vhspec {
    microts initial_value;

    /* drift_rate is given in parts per million, PPM.
       +1 PPM = +1 microsecond of drift per second (1*10^6 microseconds) */
    int drift_rate;
} vhspec;

/* Specification for a software clock. */
typedef struct scspec {
    microts amortization_period;
    //microts rapport;
    microts rapport_master;
    microts rapport_local;
    microts rapport_vhc;
    vhspec *vhclock;
} scspec;

int software_clock_gettime(scspec *v, microts *result);
int virtual_hardware_clock_gettime(vhspec *v, microts *result);
int virtual_hardware_clock_init(vhspec *v);
int real_hardware_clock_gettime(microts *result);

#endif // SCLOCK_H
