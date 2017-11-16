#include <time.h>
#include <stdio.h>

/* C(t) = H(t) + A(t)
   = H(t) + m * H(t) + N */

long get_software_clock()
{
    struct timespec hardware_monotonic;
    return 1;
}

int main(int argc, char *argv[])
{
    struct timespec monotonic;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &monotonic) == 0)
    {
        printf("Time in seconds: %ld\n", monotonic.tv_sec);
        printf("Time in nanoseconds: %ld\n", monotonic.tv_nsec);
    }
    else
    {
        printf("An unknown error occurred.\n");
    }
}
