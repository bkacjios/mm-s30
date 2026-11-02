#ifndef S30_MAP_H
#define S30_MAP_H

/*
 * S30: danielryb's Map Overhaul for Majora's Mask: Recompiled (CC0, https://github.com/danielryb/MMRecompMapOverhaul),
 * built into the port: the world map in the pause menu with the MMN64HD pack's painted map and its overlays (the
 * areas as they change over the game), markers moved to match, the player's position on it, and a zoom mode (A to
 * zoom, the stick to move, C-up/C-down to zoom in and out, B to go back).
 *
 * It's on when the painted map is installed: data/hdmap/*.tex, made by pc_tools/hd_map.py from the pack's .rtz.
 * Otherwise the original world map is drawn. MM_MAP_OVERLAYS picks how the overlays follow the game:
 * PROGRESSIVE (default), REALTIME, COMBINED or STATIC (the mod's config option).
 */
#include "ultra64.h"

struct PlayState;
struct PauseContext;
struct Actor;

extern s32 gS30MapOverhaul; // the painted map is loaded: the overhaul's functions replace the original ones

void S30Map_Init(void); // loads the painted map, once

// The original functions the mod replaces (called by them when gS30MapOverhaul)
void MapOverhaul_UpdateWorldMapCursor(struct PlayState* play);
void MapOverhaul_DrawWorldMap(struct PlayState* play);
void MapOverhaul_Minimap_DrawActorIcon(struct PlayState* play, struct Actor* actor);
s32 MapOverhaul_HideInfoPanel(struct PauseContext* pauseCtx); // the map is zoomed in: no info panel

// The mod's hooks, called at the start (on_) and end (after_) of these functions when gS30MapOverhaul (its after_
// hooks for Map_Init and Map_InitRoomData only print, in its debug build)
void on_KaleidoScope_UpdateCursorSize(struct PlayState* play);
void on_KaleidoScope_DrawInfoPanel(struct PlayState* play);
void after_KaleidoScope_DrawInfoPanel(void);
void on_KaleidoScope_UpdateNamePanel(struct PlayState* play);
void after_KaleidoScope_UpdateNamePanel(void);
void on_Map_InitRoomData(struct PlayState* play, s16 room);
void on_Map_Init(struct PlayState* play);
void onPlay_DrawMain(struct PlayState* play);

#endif
