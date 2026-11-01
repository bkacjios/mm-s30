#include "z64actor_dlftbls.h"

#include "fault.h"

// Segment and Profile declarations (also used in the table below)
#ifdef LINUX
// Actors are linked in: no overlay segments
#define DEFINE_ACTOR(name, _enumValue, _allocType, _debugName) extern struct ActorProfile name##_Profile;
#else
#define DEFINE_ACTOR(name, _enumValue, _allocType, _debugName) \
    extern struct ActorProfile name##_Profile;                 \
    DECLARE_OVERLAY_SEGMENT(name)
#endif
#define DEFINE_ACTOR_INTERNAL(name, _enumValue, _allocType, _debugName) extern struct ActorProfile name##_Profile;
#define DEFINE_ACTOR_UNSET(_enumValue)

#include "tables/actor_table.h"

#undef DEFINE_ACTOR
#undef DEFINE_ACTOR_INTERNAL
#undef DEFINE_ACTOR_UNSET

// Actor Overlay Table definition
#ifdef LINUX
// Every actor is internal (linked in), as the ones that live in code
#define DEFINE_ACTOR(name, _enumValue, allocType, _debugName)                  \
    {                                                                          \
        ROM_FILE_UNSET, NULL, NULL, NULL, &name##_Profile, NULL, allocType, 0, \
    },
#else
#define DEFINE_ACTOR(name, _enumValue, allocType, _debugName) \
    {                                                         \
        ROM_FILE(ovl_##name),                                 \
        SEGMENT_START(ovl_##name),                            \
        SEGMENT_END(ovl_##name),                              \
        NULL,                                                 \
        &name##_Profile,                                      \
        NULL,                                                 \
        allocType,                                            \
        0,                                                    \
    },
#endif

#define DEFINE_ACTOR_INTERNAL(name, _enumValue, allocType, _debugName)         \
    {                                                                          \
        ROM_FILE_UNSET, NULL, NULL, NULL, &name##_Profile, NULL, allocType, 0, \
    },

#define DEFINE_ACTOR_UNSET(_enumValue) { 0 },

ActorOverlay gActorOverlayTable[] = {
#include "tables/actor_table.h"
};

#undef DEFINE_ACTOR
#undef DEFINE_ACTOR_INTERNAL
#undef DEFINE_ACTOR_UNSET

ActorId gMaxActorId = 0;

FaultClient sActorOverlayTableFaultClient;
FaultAddrConvClient sActorOverlayTableFaultAddrConvClient;

void ActorOverlayTable_FaultClient(void* arg0, void* arg1) {
    ActorOverlay* overlayEntry;
    u32 overlaySize;
    ActorId actorId;

    FaultDrawer_SetCharPad(-2, 0);

    FaultDrawer_Printf("actor_dlftbls %u\n", gMaxActorId);
    FaultDrawer_Printf("No. RamStart- RamEnd cn  Name\n");

    for (actorId = 0, overlayEntry = &gActorOverlayTable[0]; actorId < gMaxActorId; actorId++, overlayEntry++) {
        overlaySize = (uintptr_t)overlayEntry->vramEnd - (uintptr_t)overlayEntry->vramStart;
        if (overlayEntry->loadedRamAddr != NULL) {
            FaultDrawer_Printf("%3d %08x-%08x %3d %s\n", actorId, overlayEntry->loadedRamAddr,
                               (u32)overlayEntry->loadedRamAddr + overlaySize, overlayEntry->numLoaded, "");
        }
    }
}

uintptr_t ActorOverlayTable_FaultAddrConv(uintptr_t address, void* param) {
    uintptr_t addr = address;
    ActorOverlay* actorOvl = &gActorOverlayTable[0];
    uintptr_t ramConv;
    void* ramStart;
    size_t diff;
    ActorId actorId;

    for (actorId = 0; actorId < gMaxActorId; actorId++, actorOvl++) {
        diff = (uintptr_t)actorOvl->vramEnd - (uintptr_t)actorOvl->vramStart;
        ramStart = actorOvl->loadedRamAddr;
        ramConv = (uintptr_t)actorOvl->vramStart - (uintptr_t)ramStart;

        if (ramStart != NULL) {
            if ((addr >= (uintptr_t)ramStart) && (addr < (uintptr_t)ramStart + diff)) {
                return addr + ramConv;
            }
        }
    }

    return 0;
}

#ifdef LINUX
/*
 * Overlay statics. On the N64 an overlay's .data and .bss come fresh from ROM
 * each time it is loaded (first instance spawned after the last one went), and
 * actors rely on it: e.g. En_Slime converts its texture pointers once per
 * load into the object's current address. Here actors are linked in, so the
 * build puts each overlay's .data and .bss in sections of its own
 * (Makefile.s30) and they are reset on load: .data from a copy taken the first
 * time, .bss zeroed.
 */
#include <stdlib.h>
#include <string.h>

typedef struct OverlayStatics {
    u8* data;
    u8* dataEnd;
    u8* bss;
    u8* bssEnd;
} OverlayStatics;

#define DEFINE_ACTOR(name, _enumValue, _allocType, _debugName)                                   \
    extern u8 __start_ovl_data_##name[] __attribute__((weak));                                   \
    extern u8 __stop_ovl_data_##name[] __attribute__((weak));                                    \
    extern u8 __start_ovl_bss_##name[] __attribute__((weak));                                    \
    extern u8 __stop_ovl_bss_##name[] __attribute__((weak));
#define DEFINE_ACTOR_INTERNAL(_name, _enumValue, _allocType, _debugName)
#define DEFINE_ACTOR_UNSET(_enumValue)
#include "tables/actor_table.h"
#undef DEFINE_ACTOR
#undef DEFINE_ACTOR_INTERNAL
#undef DEFINE_ACTOR_UNSET

#define DEFINE_ACTOR(name, _enumValue, _allocType, _debugName) \
    { __start_ovl_data_##name, __stop_ovl_data_##name, __start_ovl_bss_##name, __stop_ovl_bss_##name },
#define DEFINE_ACTOR_INTERNAL(_name, _enumValue, _allocType, _debugName) { 0 },
#define DEFINE_ACTOR_UNSET(_enumValue) { 0 },
static const OverlayStatics sOverlayStatics[] = {
#include "tables/actor_table.h"
};
#undef DEFINE_ACTOR
#undef DEFINE_ACTOR_INTERNAL
#undef DEFINE_ACTOR_UNSET

static u8* sOverlayDataCopy[ACTOR_ID_MAX];
static u8 sOverlayLoaded[ACTOR_ID_MAX];

void ActorOverlayTable_LoadStatics(ActorId id) {
    const OverlayStatics* ovl = &sOverlayStatics[id];
    size_t dataSize = ovl->dataEnd - ovl->data;

    if (sOverlayLoaded[id]) {
        return;
    }
    sOverlayLoaded[id] = true;
    if (dataSize != 0) {
        if (sOverlayDataCopy[id] == NULL) {
            // First load: .data is still as linked
            sOverlayDataCopy[id] = malloc(dataSize);
            if (sOverlayDataCopy[id] != NULL) {
                memcpy(sOverlayDataCopy[id], ovl->data, dataSize);
            }
        } else {
            memcpy(ovl->data, sOverlayDataCopy[id], dataSize);
        }
    }
    if (ovl->bssEnd != ovl->bss) {
        memset(ovl->bss, 0, ovl->bssEnd - ovl->bss);
    }
}

void ActorOverlayTable_UnloadStatics(ActorId id) {
    sOverlayLoaded[id] = false;
}
#endif

void ActorOverlayTable_Init(void) {
    gMaxActorId = ACTOR_ID_MAX;
    Fault_AddClient(&sActorOverlayTableFaultClient, ActorOverlayTable_FaultClient, NULL, NULL);
    Fault_AddAddrConvClient(&sActorOverlayTableFaultAddrConvClient, ActorOverlayTable_FaultAddrConv, NULL);
}

void ActorOverlayTable_Cleanup(void) {
#ifdef LINUX
    memset(sOverlayLoaded, 0, sizeof(sOverlayLoaded)); // the play state's arena goes, every overlay with it
#endif
    Fault_RemoveClient(&sActorOverlayTableFaultClient);
    Fault_RemoveAddrConvClient(&sActorOverlayTableFaultAddrConvClient);
    gMaxActorId = 0;
}
