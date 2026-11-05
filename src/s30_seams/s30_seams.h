#ifndef S30_SEAMS_H
#define S30_SEAMS_H

/*
 * S30: Reonu's Seam Fixer for Majora's Mask: Recompiled (CC0, https://github.com/Reonu/mm-seam-fixer, 1.1.0),
 * built into the port: the seam in the South Clock Town ramp, and the Swamp Volcano in Termina Field clipping
 * into the trees next to it (both of its models moved). Also the unused cleared Swamp Volcano, shown once Woodfall
 * Temple is cleared (in BgBreakwall_Init, z_bg_breakwall.c).
 *
 * The mod's hooks are plain functions here, called where the recomp would: after a room is loaded
 * (Room_ProcessRoomRequest, z_room.c) and after a file is read from the ROM (DmaMgr_RequestSync and
 * DmaMgr_RequestAsync, src/linux/reimpl.c).
 */
#include "ultra64.h"

struct PlayState;
struct RoomContext;

void SeamFixer_RoomLoaded(struct PlayState* play, struct RoomContext* roomCtx);
void SeamFixer_FileLoaded(void* ram, uintptr_t vrom, size_t size);

#endif
