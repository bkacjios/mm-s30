/*
 * Majora's Mask on the PocketGo S30: process entry.
 *
 * Environment:
 *   MM_DATA      asset segment directory (default: "data" next to the binary)
 *   MM_SAVE      save file (default: $XDG_DATA_HOME/mm/mm.sav, i.e.
 *                ~/.local/share/mm/mm.sav; else mm.sav next to the binary)
 *   MM_NOAUDIO   set to run without sound
 *
 * Crash reports also go to crash.log next to the binary (on the SD card;
 * stderr is the launcher's log in /tmp, lost on reboot).
 */
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ultra64.h"
#include "s30_audio.h"
#include "s30_app.h"
#include "s30_display.h"
#include "s30_gfx.h"
#include "s30_input.h"
#include "s30_os.h"
#include "s30_platform.h"


const char* gS30DataDir;
const char* gS30SaveFile;

extern void Main(void* arg);
extern void AudioRom_LoadAll(void);
extern const S30ButtonMap gS30Buttons[];
extern const int gS30ButtonCount;
extern uintptr_t gSegments[];
extern void* AudioThread_Update(void); // returns AudioTask* (the audio headers aren't included here)

/* The audio thread's update (the shared library's audio thread, audio lock held) */
static void audio_update(void) {
    AudioThread_Update();
}

void S30_StartAudio(void) {
    S30Audio_Init(audio_update);
}


/* Texture data as u64 words, but the pictograph (src/code/z_play.c) */
static int tex_elem_size(uintptr_t addr) {
    return S30_TexElemSize(addr);
}

int main(int argc, char** argv) {
    char* exe = strdup(argv[0]);
    const char* exeDir = dirname(exe);

    setvbuf(stdout, NULL, _IOLBF, 0); // logs survive a crash
    S30App_InstallCrashHandler(S30App_PathIn(exeDir, "crash.log"));

    gS30DataDir = getenv("MM_DATA") ? getenv("MM_DATA") : S30App_PathIn(exeDir, "data");
    if (getenv("MM_SAVE")) {
        gS30SaveFile = getenv("MM_SAVE");
    } else {
        gS30SaveFile = S30App_SavePath("mm", "mm.sav", exeDir);
    }
    gS30EnableAudio = getenv("MM_NOAUDIO") == NULL;
    printf("mm: data %s, save %s, audio %s\n", gS30DataDir, gS30SaveFile, gS30EnableAudio ? "on" : "off");
    {
        // The game loop runs on this thread (Main); the game's threads stay
        // off. ROM data comes through DmaMgr (src/linux/reimpl.c), the save
        // through src/code/sys_flashrom.c.
        S30OsConfig os = {
            .noWait = 1, // no scheduler thread to wait for (Sched_FlushTaskQueue): an empty queue fails
        };
        S30Os_Init(&os);
    }

    {
        // F3DEX2, float Mtx (the port's guMtxF2L), textures as u64 arrays but
        // the pictograph (tex_elem_size)
        S30GfxConfig gfx = { S30_UCODE_F3DEX2, (uint32_t*)gSegments, 1, tex_elem_size, "MM" };
        S30Gfx_Configure(&gfx);
    }
    S30Display_Init();
    S30Gfx_Init();
    S30Input_Init(gS30Buttons, gS30ButtonCount);
    if (gS30EnableAudio) {
        AudioRom_LoadAll();
    }

    Main(NULL);
    return 0;
}
