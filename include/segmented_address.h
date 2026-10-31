#ifndef SEGMENTED_ADDRESS_H
#define SEGMENTED_ADDRESS_H

#include "ultra64.h"

#include "stdint.h"

extern uintptr_t gSegments[NUM_SEGMENTS];

#ifdef LINUX
/*
 * Segments 0x00-0x0F resolve through gSegments; anything else is already a
 * real pointer (the port's image and heap sit at 0x40000000 and up) and
 * passes through, e.g. an overlay's own static Vtx/Gfx (as in ports/oot)
 */
static inline void* segmented_to_k0(uintptr_t addr) {
    if ((addr >> 24) > 0x0F) {
        return (void*)addr;
    }
    return (void*)(gSegments[addr >> 24] + (addr & 0x00FFFFFF));
}
#define SEGMENTED_TO_K0(addr) segmented_to_k0((uintptr_t)(addr))
#else
#define SEGMENTED_TO_K0(addr) (void*)((gSegments[SEGMENT_NUMBER(addr)] + K0BASE) + SEGMENT_OFFSET(addr))
#endif

#endif
