#ifndef _OPTIONS_MENU_H
#define _OPTIONS_MENU_H

#if defined(TARGET_SDL)
// Options screen opened from the title menu; saves to ultima5.ini on leaving.
void OPTIONS_MENU_Run(void);

// The same screen opened with F1 during play; restores the game screen after.
void OPTIONS_MENU_RunInGame(void);
#endif

#endif
