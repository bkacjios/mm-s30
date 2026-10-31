#ifndef S30_AUDIO_H
#define S30_AUDIO_H

#include "ultra64.h"

// ALSA output for the audio thread (src/s30/audio.c). The game's audio update
// renders separate L/R buffers at 32 kHz; S30Audio_Play blocks while the ALSA
// buffer is full, which paces the audio thread like the N64 AI interrupt.
extern int gS30EnableAudio;

void S30Audio_Init(void);
u32 S30Audio_QueuedFrames(void);
void S30Audio_Play(const s16* left, const s16* right, s32 frames);
// An interleaved stereo buffer (MM's synthesis output)
void S30Audio_PlayInterleaved(const s16* samples, s32 frames);
// Held by the audio thread while it updates and by the game thread around
// Audio_Update: on the N64 the two never ran at the same time, and both
// write the same audio state (sequence player flags share bytes)
void S30Audio_Lock(void);
void S30Audio_Unlock(void);

#endif
