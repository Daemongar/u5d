#ifndef _COMBAT_H
#define _COMBAT_H

#include "structs.h"

int COMBAT_0000(int param_1, int param_2, int param_3);
int COMBAT_0b94_MainLoop(void);
int COMBAT_111a(int param_1, int param_2);
int COMBAT_120e(void);
void COMBAT_1236(int param_1);
int COMBAT_13e2(int param_1, int param_2);
int COMBAT_14d6(int param_1, int param_2, int param_3, int param_4);
int COMBAT_1574(int param_1, int param_2);
void COMBAT_18ba(int param_1, int param_2);
void COMBAT_194a(int param_1, int param_2);
int COMBAT_1a5c(int param_1);

// Awards `amount` experience for a kill. `killer` is whichever party member's
// action caused the kill (may be NULL if the kill can't be attributed to a
// player). With ENABLE_PARTY_EXP, every living party member earns the
// experience instead of just `killer`; otherwise this is exactly the
// original single-recipient behavior.
void COMBAT_AwardExp(S_55a8* killer, int amount);

#endif
