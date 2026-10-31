#include "ultra64.h"

OSId osGetThreadId(OSThread* t) {
#ifdef LINUX
    return 1;
#else
    if (t == NULL) {
        t = __osRunningThread;
    }
    return t->id;
#endif
}
