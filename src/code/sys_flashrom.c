#include "sys_flashrom.h"

#include "libc64/malloc.h"
#include "PR/os_internal_flash.h"

#include "fault.h"
#include "macros.h"
#include "stack.h"
#include "libu64/stackcheck.h"
#include "z64thread.h"

OSMesgQueue sFlashromMesgQueue;
OSMesg sFlashromMesg[1];
s32 sFlashromIsInit;
s32 sFlashromVendor;

STACK(sSysFlashromStack, 0x1000);
StackEntry sSysFlashromStackInfo;
OSThread sSysFlashromThread;
FlashromRequest sFlashromRequest;

#ifdef LINUX
// No flash chip: the 128 KiB flash image lives in RAM and in the save file
// (gS30SaveFile), rewritten whole after every write. A missing file reads as
// erased flash (all 0xFF), which the save code treats as no saves.
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "s30_platform.h"

static u8 sFlashImage[FLASH_SIZE];

static void SysFlashrom_Persist(void) {
    char tmp[512];
    FILE* f;

    snprintf(tmp, sizeof(tmp), "%s.tmp", gS30SaveFile);
    f = fopen(tmp, "wb");
    if (f == NULL) {
        perror(tmp);
        return;
    }
    if (fwrite(sFlashImage, 1, sizeof(sFlashImage), f) != sizeof(sFlashImage) || fflush(f) != 0) {
        perror(tmp);
        fclose(f);
        return;
    }
    fsync(fileno(f));
    fclose(f);
    rename(tmp, gS30SaveFile);
}

s32 SysFlashrom_IsInit(void) {
    return sFlashromIsInit;
}

const char* SysFlashrom_GetVendorStr(void) {
    return "FILE";
}

s32 SysFlashrom_CheckFlashType(void) {
    return 0;
}

s32 SysFlashrom_InitFlash(void) {
    FILE* f;

    if (sFlashromIsInit) {
        return 0;
    }
    memset(sFlashImage, 0xFF, sizeof(sFlashImage));
    f = fopen(gS30SaveFile, "rb");
    if (f != NULL) {
        size_t n = fread(sFlashImage, 1, sizeof(sFlashImage), f);

        fclose(f);
        osSyncPrintf("flash: %s, %u bytes\n", gS30SaveFile, (u32)n);
    } else {
        osSyncPrintf("flash: %s not found, starting blank\n", gS30SaveFile);
    }
    sFlashromIsInit = true;
    return 0;
}

static s32 SysFlashrom_InRange(u32 pageNum, u32 pageCount) {
    return (pageNum + pageCount) * FLASH_BLOCK_SIZE <= sizeof(sFlashImage);
}

s32 SysFlashrom_Read(void* addr, u32 pageNum, u32 pageCount) {
    if (!sFlashromIsInit || !SysFlashrom_InRange(pageNum, pageCount)) {
        return -1;
    }
    memcpy(addr, &sFlashImage[pageNum * FLASH_BLOCK_SIZE], pageCount * FLASH_BLOCK_SIZE);
    return 0;
}

s32 SysFlashrom_EraseSector(u32 page) {
    // A sector is 128 pages (16 KiB)
    page &= ~0x7F;
    if (!sFlashromIsInit || !SysFlashrom_InRange(page, 0x80)) {
        return -1;
    }
    memset(&sFlashImage[page * FLASH_BLOCK_SIZE], 0xFF, 0x80 * FLASH_BLOCK_SIZE);
    SysFlashrom_Persist();
    return 0;
}

s32 SysFlashrom_Write(void* addr, u32 pageNum, u32 pageCount) {
    if (!sFlashromIsInit || !SysFlashrom_InRange(pageNum, pageCount)) {
        return -1;
    }
    memcpy(&sFlashImage[pageNum * FLASH_BLOCK_SIZE], addr, pageCount * FLASH_BLOCK_SIZE);
    SysFlashrom_Persist();
    return 0;
}

s32 SysFlashrom_ExecWrite(void* addr, u32 pageNum, u32 pageCount) {
    return SysFlashrom_Write(addr, pageNum, pageCount);
}

s32 SysFlashrom_AttemptWrite(void* addr, u32 pageNum, u32 pageCount) {
    return SysFlashrom_Write(addr, pageNum, pageCount);
}

// No flashrom thread: the write happens now and its result is posted for
// SysFlashrom_IsBusy (true once the result is in) and SysFlashrom_AwaitResult
void SysFlashrom_WriteAsync(void* addr, u32 pageNum, u32 pageCount) {
    FlashromRequest* req = &sFlashromRequest;

    if (sFlashromIsInit) {
        req->type = FLASHROM_REQUEST_WRITE;
        req->addr = addr;
        req->pageNum = pageNum;
        req->pageCount = pageCount;
        osCreateMesgQueue(&req->queue, req->msgBuf, ARRAY_COUNT(req->msgBuf));
        req->response = SysFlashrom_Write(addr, pageNum, pageCount);
        osSendMesg(&req->queue, (OSMesg)(intptr_t)req->response, OS_MESG_NOBLOCK);
    }
}

s32 SysFlashrom_IsBusy(void) {
    FlashromRequest* req = &sFlashromRequest;

    if (!sFlashromIsInit) {
        return -1;
    }
    return MQ_IS_FULL(&req->queue);
}

s32 SysFlashrom_AwaitResult(void) {
    FlashromRequest* req = &sFlashromRequest;

    if (!sFlashromIsInit) {
        return -1;
    }
    osRecvMesg(&req->queue, NULL, OS_MESG_NOBLOCK);
    return req->response;
}

void SysFlashrom_WriteSync(void* addr, u32 pageNum, u32 pageCount) {
    SysFlashrom_WriteAsync(addr, pageNum, pageCount);
    SysFlashrom_AwaitResult();
}
#else
s32 SysFlashrom_IsInit(void) {
    return sFlashromIsInit;
}

const char* SysFlashrom_GetVendorStr(void) {
    switch (sFlashromVendor) {
        case FLASH_VERSION_MX_PROTO_A:
            return "PROTO A";

        case FLASH_VERSION_MX_A:
            return "A";

        case FLASH_VERSION_MX_C:
            return "C";

        case FLASH_VERSION_MEI:
            return "D(NEW)";

        case FLASH_VERSION_MX_B_AND_D:
            return "B or D";

        default:
            return "UNKNOWN";
    }
}

s32 SysFlashrom_CheckFlashType(void) {
    u32 flashType;
    u32 flashVendor;

    if (!SysFlashrom_IsInit()) {
        return -1;
    }
    osFlashReadId(&flashType, &flashVendor);
    sFlashromVendor = flashVendor;
    if (flashType == FLASH_TYPE_MAGIC) {
        if ((flashVendor == FLASH_VERSION_MX_PROTO_A) || (flashVendor == FLASH_VERSION_MX_A) ||
            (flashVendor == FLASH_VERSION_MX_C) || (flashVendor == FLASH_VERSION_MEI) ||
            (flashVendor == FLASH_VERSION_MX_B_AND_D)) {
            return 0;
        } else {
            return -1;
        }
    }
    return -1;
}

s32 SysFlashrom_InitFlash(void) {
    osCreateMesgQueue(&sFlashromMesgQueue, sFlashromMesg, ARRAY_COUNT(sFlashromMesg));
    osFlashInit();
    sFlashromIsInit = true;
    if (SysFlashrom_CheckFlashType() != 0) {
        sFlashromIsInit = false;
        return -1;
    }
    return 0;
}

s32 SysFlashrom_Read(void* addr, u32 pageNum, u32 pageCount) {
    OSIoMesg msg;

    if (!SysFlashrom_IsInit()) {
        return -1;
    }
    osInvalDCache(addr, pageCount * FLASH_BLOCK_SIZE);
    osFlashReadArray(&msg, OS_MESG_PRI_NORMAL, pageNum, addr, pageCount, &sFlashromMesgQueue);
    osRecvMesg(&sFlashromMesgQueue, NULL, OS_MESG_BLOCK);
    return 0;
}

s32 SysFlashrom_EraseSector(u32 page) {
    if (!SysFlashrom_IsInit()) {
        return -1;
    }
    return osFlashSectorErase(page);
}

s32 SysFlashrom_ExecWrite(void* addr, u32 pageNum, u32 pageCount) {
    OSIoMesg msg;
    s32 result;
    u32 i;

    if (!SysFlashrom_IsInit()) {
        return -1;
    }
    // Ensure the page is always aligned to a sector boundary.
    if ((pageNum % FLASH_BLOCK_SIZE) != 0) {
        Fault_AddHungupAndCrash("../sys_flashrom.c", 275);
    }
    osWritebackDCache(addr, pageCount * FLASH_BLOCK_SIZE);
    for (i = 0; i < pageCount; i++) {
        osFlashWriteBuffer(&msg, OS_MESG_PRI_NORMAL, (u8*)addr + i * FLASH_BLOCK_SIZE, &sFlashromMesgQueue);
        osRecvMesg(&sFlashromMesgQueue, NULL, OS_MESG_BLOCK);
        result = osFlashWriteArray(pageNum + i);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

s32 SysFlashrom_AttemptWrite(void* addr, u32 pageNum, u32 pageCount) {
    s32 result;
    s32 i;

    if (!SysFlashrom_IsInit()) {
        return -1;
    }
    osWritebackDCache(addr, pageCount * FLASH_BLOCK_SIZE);
    i = 0;
failRetry:
    result = SysFlashrom_EraseSector(pageNum);
    if (result != 0) {
        if (i < 3) {
            i++;
            goto failRetry;
        }
        return result;
    }
    result = SysFlashrom_ExecWrite(addr, pageNum, pageCount);
    if (result != 0) {
        if (i < 3) {
            i++;
            goto failRetry;
        }
        return result;
    }
    return 0;
}

// Flash bits can only by flipped with a write from 1 -> 0. Going from 0 -> 1 requires an erasure.
// This function will try to determine if any sectors need to be erased before writing saving erase cycles.
s32 SysFlashrom_NeedsToErase(void* data, void* addr, u32 pageCount) {
    u32 size;
    u32 i;

    for (i = 0; i < pageCount * FLASH_BLOCK_SIZE; i += 4) {
        if ((*(s32*)data & *(s32*)addr) != *(s32*)addr) {
            return false;
        }
    }
    return true;
}

s32 SysFlashrom_Write(void* addr, u32 pageNum, u32 pageCount) {
    void* data;
    size_t size;
    s32 ret;

    if (!SysFlashrom_IsInit()) {
        return -1;
    }
    size = pageCount * FLASH_BLOCK_SIZE;
    data = malloc(size);
    if (data == NULL) {
        ret = SysFlashrom_AttemptWrite(addr, pageNum, pageCount);
    } else {
        SysFlashrom_Read(data, pageNum, pageCount);
        if (bcmp(data, addr, size) == 0) {
            ret = 0;
        } else {
            // Will always erase the sector even if it wouldn't normally need to.
            if (SysFlashrom_NeedsToErase(data, addr, pageCount)) {
                ret = SysFlashrom_AttemptWrite(addr, pageNum, pageCount);
            } else {
                ret = SysFlashrom_AttemptWrite(addr, pageNum, pageCount);
            }
            if (ret == 0) {
                SysFlashrom_Read(data, pageNum, pageCount);
                if (bcmp(data, addr, size) == 0) {
                    ret = 0;
                } else {
                    ret = -1;
                }
            }
        }
        free(data);
    }
    return ret;
}

void SysFlashrom_ThreadEntry(void* arg) {
    FlashromRequest* req = (FlashromRequest*)arg;

    switch (req->type) {
        case FLASHROM_REQUEST_WRITE:
            req->response = SysFlashrom_Write(req->addr, req->pageNum, req->pageCount);
            osSendMesg(&req->queue, (OSMesg)req->response, OS_MESG_BLOCK);
            break;

        case FLASHROM_REQUEST_READ:
            req->response = SysFlashrom_Read(req->addr, req->pageNum, req->pageCount);
            osSendMesg(&req->queue, (OSMesg)req->response, OS_MESG_BLOCK);
            break;
    }
}

void SysFlashrom_WriteAsync(void* addr, u32 pageNum, u32 pageCount) {
    FlashromRequest* req = &sFlashromRequest;

    if (SysFlashrom_IsInit()) {
        req->type = FLASHROM_REQUEST_WRITE;
        req->addr = addr;
        req->pageNum = pageNum;
        req->pageCount = pageCount;
        osCreateMesgQueue(&req->queue, req->msgBuf, ARRAY_COUNT(req->msgBuf));
        StackCheck_Init(&sSysFlashromStackInfo, sSysFlashromStack, STACK_TOP(sSysFlashromStack), 0, 0x100,
                        "sys_flashrom");
        osCreateThread(&sSysFlashromThread, Z_THREAD_ID_FLASHROM, SysFlashrom_ThreadEntry, req,
                       STACK_TOP(sSysFlashromStack), Z_PRIORITY_FLASHROM);
        osStartThread(&sSysFlashromThread);
    }
}

s32 SysFlashrom_IsBusy(void) {
    FlashromRequest* req = &sFlashromRequest;

    if (!SysFlashrom_IsInit()) {
        return -1;
    }
    return MQ_IS_FULL(&req->queue);
}

s32 SysFlashrom_AwaitResult(void) {
    FlashromRequest* req = &sFlashromRequest;

    if (!SysFlashrom_IsInit()) {
        return -1;
    }
    osRecvMesg(&req->queue, NULL, OS_MESG_BLOCK);
    osDestroyThread(&sSysFlashromThread);
    StackCheck_Cleanup(&sSysFlashromStackInfo);
    return req->response;
}

void SysFlashrom_WriteSync(void* addr, u32 pageNum, u32 pageCount) {
    SysFlashrom_WriteAsync(addr, pageNum, pageCount);
    SysFlashrom_AwaitResult();
}
#endif
