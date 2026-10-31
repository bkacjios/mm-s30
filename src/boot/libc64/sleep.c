#include "libc64/sleep.h"
#include "macros.h" // for ARRAY_COUNT
#ifdef LINUX
#include <time.h>
#endif

void csleep(OSTime time) {
#ifdef LINUX
    // Nothing services the libultra timer list on Linux, so a timer + message
    // wait returns at once (and leaves the timer linked): sleep for real
    u64 ns = time * 1000000000ull / OS_CPU_COUNTER;
    struct timespec ts = { (time_t)(ns / 1000000000ull), (long)(ns % 1000000000ull) };

    while (nanosleep(&ts, &ts) != 0) {
    }
#else
    OSMesgQueue mq;
    OSMesg msg[1];
    OSTimer timer;

    osCreateMesgQueue(&mq, msg, ARRAY_COUNT(msg));
    osSetTimer(&timer, time, 0, &mq, NULL);
    osRecvMesg(&mq, NULL, OS_MESG_BLOCK);
#endif
}

void nsleep(u32 nsec) {
    csleep(OS_NSEC_TO_CYCLES(nsec));
}

void usleep(u32 usec) {
    csleep(OS_USEC_TO_CYCLES(usec));
}

void msleep(u32 msec) {
    csleep((msec * OS_CPU_COUNTER) / 1000ULL);
}

void sleep(u32 sec) {
    csleep(sec * OS_CPU_COUNTER);
}
