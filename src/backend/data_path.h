#ifndef _BACKEND_DATA_PATH_H
#define _BACKEND_DATA_PATH_H

#if defined(TARGET_SDL)
// Find the Ultima V data folder and make it the working directory: the current
// folder, --data <folder>, the folder remembered from an earlier run, or one the
// player picks. Returns false if none was found (the player has been told why).
bool DATAPATH_Locate(int argc, char** argv);
#endif

#endif
