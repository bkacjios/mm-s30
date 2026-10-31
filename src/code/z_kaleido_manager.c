#include "kaleido_manager.h"

#include "global.h"
#include "fault.h"
#include "libu64/loadfragment.h"

#ifdef LINUX
// The overlays are linked into the executable: no ROM file, no VRAM range
#define KALEIDO_OVERLAY(name) { NULL, ROM_FILE_UNSET, NULL, NULL, 0, #name }
#else
#define KALEIDO_OVERLAY(name) \
    { NULL, ROM_FILE(ovl_##name), SEGMENT_START(ovl_##name), SEGMENT_END(ovl_##name), 0, #name }
#endif

KaleidoMgrOverlay gKaleidoMgrOverlayTable[KALEIDO_OVL_MAX] = {
    KALEIDO_OVERLAY(kaleido_scope),
    KALEIDO_OVERLAY(player_actor),
};

void* sKaleidoAreaPtr = NULL;
KaleidoMgrOverlay* gKaleidoMgrCurOvl = NULL;

FaultAddrConvClient sKaleidoMgrFaultAddrConvClient;

uintptr_t KaleidoManager_FaultAddrConv(uintptr_t address, void* param) {
    uintptr_t addr = address;
    KaleidoMgrOverlay* kaleidoMgrOvl = gKaleidoMgrCurOvl;
    uintptr_t ramConv;
    void* ramStart;
    size_t diff;

    if (kaleidoMgrOvl != NULL) {
        diff = (uintptr_t)kaleidoMgrOvl->vramEnd - (uintptr_t)kaleidoMgrOvl->vramStart;
        ramStart = kaleidoMgrOvl->loadedRamAddr;
        ramConv = (uintptr_t)kaleidoMgrOvl->vramStart - (uintptr_t)ramStart;

        if (ramStart != NULL) {
            if ((addr >= (uintptr_t)ramStart) && (addr < (uintptr_t)ramStart + diff)) {
                return addr + ramConv;
            }
        }
    }
    return 0;
}

void KaleidoManager_LoadOvl(KaleidoMgrOverlay* ovl) {
#ifdef LINUX
    // Linked in: just mark it current
    ovl->loadedRamAddr = ovl->vramStart;
    ovl->offset = 0;
    gKaleidoMgrCurOvl = ovl;
    return;
#endif
    ovl->loadedRamAddr = sKaleidoAreaPtr;
    Overlay_Load(ovl->file.vromStart, ovl->file.vromEnd, ovl->vramStart, ovl->vramEnd, ovl->loadedRamAddr);
    ovl->offset = (uintptr_t)ovl->loadedRamAddr - (uintptr_t)ovl->vramStart;
    gKaleidoMgrCurOvl = ovl;
}

void KaleidoManager_ClearOvl(KaleidoMgrOverlay* ovl) {
    if (ovl->loadedRamAddr != NULL) {
        ovl->offset = 0;
#ifndef LINUX // nothing was copied in
        bzero(ovl->loadedRamAddr, (uintptr_t)ovl->vramEnd - (uintptr_t)ovl->vramStart);
#endif
        ovl->loadedRamAddr = NULL;
        gKaleidoMgrCurOvl = NULL;
    }
}

void KaleidoManager_Init(PlayState* play) {
    s32 largestSize = 0;
    s32 size;
    u32 i;

    for (i = 0; i < ARRAY_COUNT(gKaleidoMgrOverlayTable); i++) {
        size = (uintptr_t)gKaleidoMgrOverlayTable[i].vramEnd - (uintptr_t)gKaleidoMgrOverlayTable[i].vramStart;
        if (size > largestSize) {
            largestSize = size;
        }
    }

#ifndef LINUX // no area to load overlays into
    sKaleidoAreaPtr = THA_AllocTailAlign16(&play->state.tha, largestSize);
#endif
    gKaleidoMgrCurOvl = NULL;
    Fault_AddAddrConvClient(&sKaleidoMgrFaultAddrConvClient, KaleidoManager_FaultAddrConv, NULL);
}

void KaleidoManager_Destroy(void) {
    Fault_RemoveAddrConvClient(&sKaleidoMgrFaultAddrConvClient);

    if (gKaleidoMgrCurOvl != NULL) {
        KaleidoManager_ClearOvl(gKaleidoMgrCurOvl);
        gKaleidoMgrCurOvl = NULL;
    }

    sKaleidoAreaPtr = NULL;
}

void* KaleidoManager_GetRamAddr(void* vram) {
    KaleidoMgrOverlay* ovl = gKaleidoMgrCurOvl;

#ifdef LINUX
    // Linked in: the address is already the function's
    return vram;
#endif

    if (ovl == NULL) {
        s32 i;

        for (i = 0; i < ARRAY_COUNT(gKaleidoMgrOverlayTable); i++) {
            ovl = &gKaleidoMgrOverlayTable[i];
            if (((uintptr_t)vram >= (uintptr_t)ovl->vramStart) && ((uintptr_t)ovl->vramEnd >= (uintptr_t)vram)) {
                KaleidoManager_LoadOvl(ovl);
                return (void*)((uintptr_t)vram + ovl->offset);
            }
        }
        return NULL;
    }

    if (((uintptr_t)vram < (uintptr_t)ovl->vramStart) || ((uintptr_t)vram >= (uintptr_t)ovl->vramEnd)) {
        return NULL;
    }

    return (void*)((uintptr_t)vram + ovl->offset);
}
