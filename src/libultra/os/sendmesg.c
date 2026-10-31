#include "ultra64.h"

s32 osSendMesg(OSMesgQueue* mq, OSMesg msg, s32 flags) {
#ifdef LINUX
    // No libultra threads to block on: a full queue fails (as in ports/oot)
    OSIntMask saveMask;
    s32 last;

    if (mq == NULL) {
        return -1;
    }
    saveMask = __osDisableInt();
    if (MQ_IS_FULL(mq)) {
        __osRestoreInt(saveMask);
        return -1;
    }
    last = (mq->first + mq->validCount) % mq->msgCount;
    mq->msg[last] = msg;
    mq->validCount++;
    __osRestoreInt(saveMask);
    return 0;
#else
    register u32 saveMask = __osDisableInt();
    register s32 last;

    while (MQ_IS_FULL(mq)) {
        if (flags == OS_MESG_BLOCK) {
            __osRunningThread->state = OS_STATE_WAITING;
            __osEnqueueAndYield(&mq->fullQueue);
        } else {
            __osRestoreInt(saveMask);
            return -1;
        }
    }

    last = (mq->first + mq->validCount) % mq->msgCount;

    mq->msg[last] = msg;
    mq->validCount++;

    if (mq->mtQueue->next != NULL) {
        osStartThread(__osPopThread(&mq->mtQueue));
    }

    __osRestoreInt(saveMask);
    return 0;
#endif
}
