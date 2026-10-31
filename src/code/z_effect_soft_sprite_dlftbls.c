#include "z64effect_ss.h"
#include "segment_symbols.h"

// Profile and linker symbol declarations (used in the table below)
#ifdef LINUX
// Effects are linked in: no overlay segments
#define DEFINE_EFFECT_SS(name, _enumValue) extern EffectSsProfile name##_Profile;
#else
#define DEFINE_EFFECT_SS(name, _enumValue) \
    extern EffectSsProfile name##_Profile; \
    DECLARE_OVERLAY_SEGMENT(name)
#endif

#define DEFINE_EFFECT_SS_UNSET(_enumValue)

#include "tables/effect_ss_table.h"

#undef DEFINE_EFFECT_SS
#undef DEFINE_EFFECT_SS_UNSET

#ifdef LINUX
// No VRAM range: EffectSs_Spawn uses the profile directly
#define DEFINE_EFFECT_SS(name, _enumValue) { ROM_FILE_UNSET, NULL, NULL, NULL, &name##_Profile, 1 },
#else
#define DEFINE_EFFECT_SS(name, _enumValue)                                                                  \
    {                                                                                                       \
        ROM_FILE(ovl_##name), SEGMENT_START(ovl_##name), SEGMENT_END(ovl_##name), NULL, &name##_Profile, 1, \
    },
#endif

#define DEFINE_EFFECT_SS_UNSET(_enumValue) { 0 },

EffectSsOverlay gEffectSsOverlayTable[EFFECT_SS_TYPE_MAX] = {
#include "tables/effect_ss_table.h"
};

#undef DEFINE_EFFECT_SS
#undef DEFINE_EFFECT_SS_UNSET
