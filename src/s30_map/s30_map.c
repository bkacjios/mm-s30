/*
 * S30: the map overhaul's ties to the port (s30_map.h): the recomp functions it imports, and the painted map, loaded
 * from data/hdmap/<overlay>.tex (pc_tools/hd_map.py) and drawn in place of the overlays' thumbnails
 * (S30Gfx_TextureOverride), as the recomp's texture pack does.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "map_overhaul.h"
#include "s30_display.h"

extern const char* gS30DataDir;

s32 gS30MapOverhaul = false;

// The overlays (overlay_data.c), by the names of their textures in data/hdmap
#define MAP_OVERLAYS(X)                                                                                       \
    X(WorldMap) X(CastleRoof) X(ChimneySmith) X(GBayMain) X(HookshotTree) X(IceTerminaGate) X(IkanaMain)    \
    X(IkanaRiver) X(KeetaArea) X(MikauGrave) X(PondCrystal) X(RanchBoulder) X(SakonDoor) X(Scaffolding)      \
    X(Seahorses) X(Shiro) X(SnowheadMain) X(SwampMain) X(TrackBldrIcy) X(TrackBldrThawed) X(WFTempleCursed)

#define DECLARE_OVERLAY(name) extern u64 name##OverlayTex[];
MAP_OVERLAYS(DECLARE_OVERLAY)
#undef DECLARE_OVERLAY

typedef struct {
    char magic[4]; // "S30T"
    u16 width;
    u16 height;
    u8 format; // 0 RGBA8888, 1 RGB565
    u8 pad[3];
} S30Tex;

/* Registers data/hdmap/<name>.tex for the texture `thumb`; whether it loaded */
static s32 S30Map_LoadOverlay(const char* name, const void* thumb) {
    char path[512];
    S30Tex head;
    size_t bytes;
    void* pixels;
    FILE* f;

    snprintf(path, sizeof(path), "%s/hdmap/%s.tex", gS30DataDir, name);
    f = fopen(path, "rb");
    if (f == NULL) {
        return false;
    }
    if ((fread(&head, sizeof(head), 1, f) != 1) || (memcmp(head.magic, "S30T", 4) != 0) || (head.width == 0) ||
        (head.height == 0)) {
        fclose(f);
        printf("mm: map: %s is not a texture\n", path);
        return false;
    }
    bytes = (size_t)head.width * head.height * (head.format == 1 ? 2 : 4);
    pixels = malloc(bytes); // the renderer's now: freed once uploaded (at first use)
    if ((pixels == NULL) || (fread(pixels, bytes, 1, f) != 1)) {
        free(pixels);
        fclose(f);
        printf("mm: map: %s is short\n", path);
        return false;
    }
    fclose(f);
    S30Gfx_TextureOverride(thumb, head.width, head.height, head.format == 1, pixels);
    return true;
}

void S30Map_Init(void) {
    static s32 sDone = false;

    if (sDone) {
        return;
    }
    sDone = true;

    // On with the painted map; each overlay without its texture just shows its thumbnail
    if (!S30Map_LoadOverlay("WorldMap", WorldMapOverlayTex)) {
        printf("mm: map: no painted map (%s/hdmap), the original world map\n", gS30DataDir);
        return;
    }
#define LOAD_OVERLAY(name)                                \
    if (strcmp(#name, "WorldMap") != 0) {                 \
        S30Map_LoadOverlay(#name, name##OverlayTex);      \
    }
    MAP_OVERLAYS(LOAD_OVERLAY)
#undef LOAD_OVERLAY
    gS30MapOverhaul = true;
    printf("mm: map: map overhaul on\n");
}

// The recomp's imports
s32 recomp_get_analog_cam_enabled(void) {
    return false; // no right stick
}

void recomp_get_camera_inputs(float* x, float* y) {
    *x = 0.0f;
    *y = 0.0f;
}

u32 recomp_get_config_u32(const char* key) {
    static const char* sModes[] = { "PROGRESSIVE", "REALTIME", "COMBINED", "STATIC" }; // MapOverlayMode
    const char* mode = getenv("MM_MAP_OVERLAYS");
    u32 i;

    if ((strcmp(key, "map_overlay_mode") == 0) && (mode != NULL)) {
        for (i = 0; i < ARRAY_COUNT(sModes); i++) {
            if (strcasecmp(mode, sModes[i]) == 0) {
                return i;
            }
        }
    }
    return 0;
}

double recomp_get_config_double(const char* key) {
    (void)key;
    return 0.0;
}
