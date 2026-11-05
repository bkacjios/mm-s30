/*
 * S30: Seam Fixer's src/fix_seams.c (s30_seams.h), its hooks as plain functions and BgBreakwall_Init's patch in
 * z_bg_breakwall.c
 */
#include "global.h"
#include "segmented_address.h"
#include "segment_symbols.h"
#include "s30_seams.h"
#include "volcano_fix_pos.h"
#include "volcano_cleared_fix_pos.h"

extern Vtx Z2_CLOCKTOWER_room_00Vtx_002A90[];
extern Gfx Z2_CLOCKTOWER_room_00DL_0032D0[];

#define MAX_SCAN 10000

static bool replace_dl_commands_jump(Gfx* to_scan, Gfx* to_find, Gfx* target, u32 length) {
    Gfx* cur_cmd = to_scan;
    for (u32 i = 0; i < MAX_SCAN; i++) {
        // If the current command's words match the words of the first target command, check the remaining commands.
        if (cur_cmd->words.w0 == to_find[0].words.w0 && cur_cmd->words.w1 == to_find[0].words.w1) {
            // Check the remaining commands.
            bool didMatch = true;
            for (u32 commandIndex = 1; commandIndex < length; commandIndex++) {
                if (cur_cmd[commandIndex].words.w0 != to_find[commandIndex].words.w0 ||
                    cur_cmd[commandIndex].words.w1 != to_find[commandIndex].words.w1) {
                    didMatch = false;
                    break;
                }
            }
            // If all commands matched, perform the replacement and return true.
            if (didMatch) {
                gSPDisplayList(cur_cmd, target);
                // Replace the remaining commands with noops.
                for (u32 commandIndex = 1; commandIndex < length; commandIndex++) {
                    gSPNoOp(&cur_cmd[commandIndex]);
                }
                return true;
            }
        }
        switch (cur_cmd->words.w0) {
            case G_ENDDL:
                // Reached the end of the displaylist without finding the target command.
                return false;
            case G_DL:
                // If this is a branchlist (G_DL_NOPUSH) then treat it as the end of a displaylist.
                {
                    u32 push = (cur_cmd->words.w0 >> 16) & 0xFF;
                    if (push == G_DL_NOPUSH) {
                        return false;
                    }
                }
            default:
                // Just continue forward for any other command.
                break;
        }

        // Go to the next command.
        cur_cmd++;
    }
    return false;
}

static Gfx cringeDL[] = {
    gsSPVertex(&Z2_CLOCKTOWER_room_00Vtx_002A90[14], 32, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
};

static Vtx ramp_vertices[5] = {
    { { { -393, 200, -1253 }, 0, { 1253, -2385 }, { 208, 118, 0, 255 } } },
    { { { -393, 200, -1400 }, 0, { 0, -2385 }, { 208, 118, 0, 255 } } },
    { { { -513, 151, -1253 }, 0, { 1253, -727 }, { 208, 118, 0, 255 } } },
    { { { -640, 100, -1400 }, 0, { 0, 1024 }, { 208, 118, 0, 255 } } },
    { { { -640, 100, -1253 }, 0, { 1253, 1024 }, { 208, 118, 0, 255 } } },
};

static Gfx basedRamp[] = {
    gsSPVertex(ramp_vertices + 0, 5, 0),
    gsSP2Triangles(0, 1, 2, 0, 1, 3, 2, 0),
    gsSP1Triangle(3, 4, 2, 0),
    gsSPEndDisplayList(),
};

static Gfx basedDL[] = {
    gsSPDisplayList(basedRamp),
    gsSPVertex(&Z2_CLOCKTOWER_room_00Vtx_002A90[14], 32, 0),
    gsSPEndDisplayList(),
};

// The mod's on/after_process_room_request: called when Room_ProcessRoomRequest has just loaded a room (segment 3 is
// the room)
void SeamFixer_RoomLoaded(PlayState* play, RoomContext* roomCtx) {
    // Check if it's the scene we're editing.
    if (play->sceneId == SCENE_CLOCKTOWER) {
        // Check if it's the room we're editing.
        if (roomCtx->curRoom.num == 0) {
            gSegments[0x03] = OS_K0_TO_PHYSICAL(roomCtx->curRoom.segment);
            Gfx* to_scan = (Gfx*)Lib_SegmentedToVirtual(Z2_CLOCKTOWER_room_00DL_0032D0);
            replace_dl_commands_jump(to_scan, cringeDL, basedDL, ARRAY_COUNT(cringeDL));
        }
    }
}

DECLARE_ROM_SEGMENT(object_keikoku_obj);
extern Gfx object_keikoku_obj_DL_0027D8[]; // uncleared volcano
extern Gfx object_keikoku_obj_DL_0044A8[]; // cleared volcano

// The mod's on_DmaMgr_RequestSync/after_dma: called after a file is read from the ROM (the whole object only: the
// size check is the port's)
void SeamFixer_FileLoaded(void* ram, uintptr_t vrom, size_t size) {
    if (vrom == SEGMENT_ROM_START(object_keikoku_obj) && size >= SEGMENT_ROM_SIZE(object_keikoku_obj)) {
        uintptr_t old_segment_6 = gSegments[0x06];
        gSegments[0x06] = OS_K0_TO_PHYSICAL(ram);
        Gfx* to_patch = Lib_SegmentedToVirtual(object_keikoku_obj_DL_0027D8);
        Gfx* to_patch2 = Lib_SegmentedToVirtual(object_keikoku_obj_DL_0044A8);
        gSPBranchList(to_patch, volcano_fix_pos);
        gSPBranchList(to_patch2, volcano_cleared_fix_pos);
        gSegments[0x06] = old_segment_6;
    }
}
