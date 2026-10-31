#ifndef LIBC64_SLEEP_H
#define LIBC64_SLEEP_H

#include "ultra64.h"

#ifdef LINUX
// Not the C library's sleep/usleep (which they would replace process-wide)
#define usleep Sleep_Usec
#define sleep Sleep_Sec
#endif

void csleep(OSTime time);
void nsleep(u32 nsec);
void usleep(u32 usec);
void msleep(u32 msec);
void sleep(u32 sec);

#endif
