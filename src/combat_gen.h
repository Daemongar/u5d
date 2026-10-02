#ifndef _COMBAT_GEN_H
#define _COMBAT_GEN_H

// Procedurally regenerate the terrain portion of D_ad14 for the given
// CBT map index. Party and monster spawn positions read from the CBT
// metadata are preserved. Compiles to a no-op on DOS16/MATCHING_BUILD.
void COMBAT_GEN_GenerateArena(int mapIndex);

#endif
