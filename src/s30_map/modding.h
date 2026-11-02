#ifndef __MODDING_H__
#define __MODDING_H__

/*
 * S30: the Zelda64Recomp mod framework's markers, for the map overhaul built into the port. Patches and hooks are
 * plain functions here: the game calls them where the recomp would (z_kaleido_map.c, z_kaleido_scope_NES.c,
 * z_map_exp.c, z_map_disp.c, z_play.c; see s30_map.h). Imports are declared and defined in s30_map.c.
 */
#define RECOMP_IMPORT(mod, func) func;
#define RECOMP_EXPORT
#define RECOMP_PATCH
#define RECOMP_FORCE_PATCH
#define RECOMP_HOOK(func)
#define RECOMP_HOOK_RETURN(func)

#endif
