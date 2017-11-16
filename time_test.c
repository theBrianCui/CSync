#include <time.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    struct timespec monotonic;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &monotonic) == 0) {
        printf("Time in seconds: %ld\n", monotonic.tv_sec);
        printf("Time in nanoseconds: %ld\n", monotonic.tv_nsec);
    } else {
        printf("An unknown error occurred.\n");
    }
}
