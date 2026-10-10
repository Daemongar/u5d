#include "common/common.h"
#include "common/file.h"

#include "data_path.h"

#if defined(TARGET_SDL)

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#if defined(OS_WINDOWS)
#include <direct.h>
#define chdir _chdir
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

// The game reads all of its data (and ultima5.ini and saves) relative to the
// working directory, so finding the Ultima V folder once and switching into it
// is all the rest of the code needs. Order: the current folder, --data <dir>,
// the folder remembered from an earlier run, then a folder picker.

#define PATH_MAX_LEN 1024

// A few files every copy of Ultima V has.
static const char* s_requiredFiles[] = {"BRIT.DAT", "DATA.OVL", "TILES.16", "INIT.GAM"};

static bool IsGameFolderHere(void)
{
    for (int i = 0; i < ARRAYSIZE(s_requiredFiles); i++)
    {
        FILE* fp = FILE_Open(s_requiredFiles[i], "rb");
        if (fp == NULL)
            return false;
        fclose(fp);
    }
    return true;
}

// Switch into dir if it holds the game; otherwise stay where we were.
static bool TryFolder(const char* dir)
{
    char previous[PATH_MAX_LEN];

    if (dir == NULL || dir[0] == '\0' || getcwd(previous, sizeof(previous)) == NULL)
        return false;
    if (chdir(dir) != 0)
        return false;
    if (IsGameFolderHere())
        return true;

    if (chdir(previous) != 0)
    {
        // Nowhere better to go; the caller tries the next candidate anyway.
    }
    return false;
}

static void RememberedPathFile(char* out, int size)
{
    char* prefDir = SDL_GetPrefPath("u5d", "UltimaV");
    out[0] = '\0';
    if (prefDir != NULL)
    {
        SDL_snprintf(out, size, "%sdata_path.txt", prefDir);
        SDL_free(prefDir);
    }
}

static bool LoadRememberedFolder(char* out, int size)
{
    char file[PATH_MAX_LEN];
    FILE* fp;

    RememberedPathFile(file, sizeof(file));
    if (file[0] == '\0' || (fp = fopen(file, "r")) == NULL)
        return false;

    bool ok = fgets(out, size, fp) != NULL;
    fclose(fp);
    if (ok)
        out[strcspn(out, "\r\n")] = '\0';
    return ok && out[0] != '\0';
}

static void RememberCurrentFolder(void)
{
    char file[PATH_MAX_LEN];
    char cwd[PATH_MAX_LEN];
    FILE* fp;

    RememberedPathFile(file, sizeof(file));
    if (file[0] == '\0' || getcwd(cwd, sizeof(cwd)) == NULL || (fp = fopen(file, "w")) == NULL)
        return;
    fprintf(fp, "%s\n", cwd);
    fclose(fp);
}

typedef struct FolderPick
{
    bool done;
    char path[PATH_MAX_LEN];
} FolderPick;

static void SDLCALL OnFolderPicked(void* userdata, const char* const* filelist, int filter)
{
    FolderPick* pick = (FolderPick*)userdata;
    (void)filter;

    pick->path[0] = '\0';
    if (filelist != NULL && filelist[0] != NULL)
        SDL_strlcpy(pick->path, filelist[0], sizeof(pick->path));
    pick->done = true;
}

// Show the system folder picker and wait for an answer ("" if cancelled).
static void PickFolder(char* out, int size)
{
    FolderPick pick = {false, ""};

    SDL_ShowOpenFolderDialog(OnFolderPicked, &pick, NULL, NULL, false);
    while (!pick.done)
    {
        SDL_Event ev;
        SDL_WaitEventTimeout(&ev, 50);
    }
    SDL_strlcpy(out, pick.path, size);
}

bool DATAPATH_Locate(int argc, char** argv)
{
    char path[PATH_MAX_LEN];

    if (IsGameFolderHere())
    {
        RememberCurrentFolder();
        return true;
    }

    for (int i = 1; i + 1 < argc; i++)
    {
        if (strcmp(argv[i], "--data") == 0 && TryFolder(argv[i + 1]))
        {
            RememberCurrentFolder();
            return true;
        }
    }

    if (LoadRememberedFolder(path, sizeof(path)) && TryFolder(path))
        return true;

    // Ask. The dialogs need SDL's video subsystem, which the backend starts
    // properly later; SDL counts the initializations.
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
    {
        fprintf(stderr, "Run this from your Ultima V folder, or pass --data <folder>.\n");
        return false;
    }

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Ultima V",
                             "Please choose the folder that contains your Ultima V game files "
                             "(BRIT.DAT, DATA.OVL, INIT.GAM and the rest).\n\n"
                             "They are not included with this program; you need your own copy of Ultima V.",
                             NULL);

    for (;;)
    {
        PickFolder(path, sizeof(path));
        if (path[0] == '\0')
            break; // cancelled

        if (TryFolder(path))
        {
            RememberCurrentFolder();
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            return true;
        }

        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Ultima V",
                                 "That folder doesn't contain the Ultima V game files "
                                 "(BRIT.DAT, DATA.OVL, TILES.16, INIT.GAM). Please choose another.",
                                 NULL);
    }

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Ultima V",
                             "Ultima V needs its original game files to run.\n\n"
                             "Start it again and choose your Ultima V folder, run it from that folder, "
                             "or pass --data <folder> on the command line.",
                             NULL);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return false;
}

#endif
