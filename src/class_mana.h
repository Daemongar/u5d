#ifndef _CLASS_MANA_H
#define _CLASS_MANA_H

#include "common/common.h"
#include "common/options.h"
#include "macros.h"

#if defined(ENABLE_BARD_MANA_ROUNDUP)
// Bard mana from INTEL: rounded up with the bard_mana_roundup setting, down
// (as in the original) otherwise.
#define BARD_MANA(intel) ((u8)(g_options.bardManaRoundup ? ((intel) + 1) >> 1 : (intel) >> 1))
#endif

#if defined(ENABLE_U4_CLASS_MANA)
// Maximum mana for the Ultima IV classes (Druid, Paladin, Ranger, Tinker) that
// can lead the party after an Ultima IV import. The original refilled mana only
// for the Avatar, Mage and Bard, so these never regained any. Ultima V halved
// Ultima IV's mana (Mage 2x -> 1x INT, Bard 1x -> 1/2 INT); the same halving is
// applied to the rest: Druid 3/4, Paladin and Ranger 1/2, Tinker 1/4 of INT.
// Shepherds, like Fighters, have none.
static inline u8 CLASS_U4MaxMana(u8 cls, u8 intel)
{
    switch (cls)
    {
    case CLASS_DRUID:
        return (u8)(intel * 3 / 4);
    case CLASS_PALADIN:
    case CLASS_RANGER:
#if defined(ENABLE_BARD_MANA_ROUNDUP)
        return BARD_MANA(intel);
#else
        return (u8)(intel >> 1);
#endif
    case CLASS_TINKER:
        return (u8)(intel / 4);
    default:
        return 0;
    }
}

#define CASE_U4_CLASS_MANA(character)                                                                                  \
    case CLASS_DRUID:                                                                                                  \
    case CLASS_PALADIN:                                                                                                \
    case CLASS_RANGER:                                                                                                 \
    case CLASS_TINKER:                                                                                                 \
        if (g_options.u4ClassMana)                                                                                     \
            (character).mag = CLASS_U4MaxMana((character).cls, (character).intel);                                     \
        break;
#else
#define CASE_U4_CLASS_MANA(character)
#endif

#endif
