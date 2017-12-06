#include <stdint.h>
#ifndef SCLOCK_H
#define SCLOCK_H

/* htonll and ntohll are used for converting 64-bit integers to and from host
   and network order. Unfortunately, there is no C-native way to do this.
   These portable macros were originally written by deltamind106 and AndyG
   on StackOverflow. You can view the original question and answer here:
   https://stackoverflow.com/a/28592202 */
#define htonll(x) ((1==htonl(1)) ? (x) : \
((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohll(x) ((1==ntohl(1)) ? (x) : \
((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

/* Messages are 4 + 8 = 12 bytes.
   [4 byte sequence number] [8 byte timestamp] */
#define MESSAGE_SIZE 12
static const uint32_t SEQ_NUM_SIZE = 4;
static const uint32_t PAYLOAD_SIZE = 8;

static const uint32_t MILLION = 1000000;
static const char *QUERY_STRING = "time = ?";

/* Timestamp supporting microsecond precision.
   1 microts = 1 microsecond. */
typedef int64_t microts;

/* Specification for a hardware clock.
   Initialize with a drift_rate and then calling
   virtual_hardware_clock_init to assign the initial_value. */
typedef struct vhspec {
    microts initial_value;

    /* a hard offset that adjusts the final read value.
       Used for synchronizing two vhclocks with the same drift. */
    microts offset;

    /* a vhclock synced to another may have a small sync error
       interpreted as timestamp +- error. Usually zero. */
    microts error;

    /* drift_rate is given in parts per million, PPM.
       +1 PPM = +1 microsecond of drift per second (1*10^6 microseconds) */
    double drift_rate;
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
