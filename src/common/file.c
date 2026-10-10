#include "common/common.h"

#include <stdio.h>
#include <string.h>

#if defined(ENABLE_BUG_FIX) && defined(OS_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#if defined(OS_LINUX) || defined(OS_MACOS)
#include <dirent.h>
#include <strings.h>

// The game refers to its data files in whatever case it likes ("ibm.ch" vs
// IBM.CH on disk). DOS and Windows don't care; here, resolve each path
// component against the directory listing. Components that don't exist are
// kept as given, so new files are still created with the requested name.
void FILE_ResolvePath(const char* path, char* out, int outSize)
{
    char dirPath[512];
    const char* p = path;

    out[0] = '\0';
    if (*p == '/')
    {
        strncat(out, "/", outSize - 1);
        p++;
    }

    while (*p)
    {
        const char* end = strchr(p, '/');
        int len = end ? (int)(end - p) : (int)strlen(p);
        char name[256];
        DIR* dir;

        if (len >= (int)sizeof(name))
            len = sizeof(name) - 1;
        memcpy(name, p, len);
        name[len] = '\0';

        snprintf(dirPath, sizeof(dirPath), "%s", out[0] ? out : ".");
        dir = opendir(dirPath);
        if (dir)
        {
            struct dirent* ent;
            while ((ent = readdir(dir)) != NULL)
            {
                if (!strcasecmp(ent->d_name, name))
                {
                    snprintf(name, sizeof(name), "%s", ent->d_name);
                    break;
                }
            }
            closedir(dir);
        }

        strncat(out, name, outSize - strlen(out) - 1);
        if (!end)
            break;
        strncat(out, "/", outSize - strlen(out) - 1);
        p = end + 1;
    }
}

static FILE* OpenFile(const char* path, const char* mode)
{
    char resolved[512];
    FILE* fp = fopen(path, mode);

    if (fp)
        return fp;

    FILE_ResolvePath(path, resolved, sizeof(resolved));
    return fopen(resolved, mode);
}
#else
void FILE_ResolvePath(const char* path, char* out, int outSize)
{
    strncpy(out, path, outSize - 1);
    out[outSize - 1] = '\0';
}

static FILE* OpenFile(const char* path, const char* mode) { return fopen(path, mode); }
#endif

FILE* FILE_Open(const char* path, const char* mode)
{
    FILE* fp = OpenFile(path, mode);

#if !defined(TARGET_DOS16)
    // The GOG release keeps INIT.GAM and its own SAVED.GAM in __support/save,
    // layered over the game folder by its DOSBox setup. Read from there when a
    // file isn't in the game folder itself (writes still go to the game folder).
    if (fp == NULL && mode[0] == 'r')
    {
        char gogPath[512];
        snprintf(gogPath, sizeof(gogPath), "__support" PATH_SEP "save" PATH_SEP "%s", path);
        fp = OpenFile(gogPath, mode);
    }
#endif

    return fp;
}

int FILE_ReadU32LE(FILE* fp, u32* out)
{
    u8 b[4];

    if (fread(b, 1, 4, fp) != 4)
        return 0;

    *out = (u32)b[0] | ((u32)b[1] << 8) | ((u32)b[2] << 16) | ((u32)b[3] << 24);

    return 1;
}

int FILE_ReadU16LE(FILE* fp, u16* out)
{
    u8 b[2];

    if (fread(b, 1, 2, fp) != 2)
        return 0;

    *out = (u16)b[0] | ((u16)b[1] << 8);

    return 1;
}

int FILE_ReadU8(FILE* fp, u8* out)
{
    int c = fgetc(fp);
    if (c == EOF)
        return 0;

    *out = (u8)c;

    return 1;
}

int FILE_WriteU32LE(FILE* fp, u32 in)
{
    u8 b[4];

    b[0] = (u8)in;
    b[1] = (u8)(in >> 8);
    b[2] = (u8)(in >> 16);
    b[3] = (u8)(in >> 24);

    if (fwrite(b, 1, 4, fp) != 4)
        return 0;

    return 1;
}

int FILE_WriteU16LE(FILE* fp, u16 in)
{
    u8 b[2];

    b[0] = (u8)in;
    b[1] = (u8)(in >> 8);

    if (fwrite(b, 1, 2, fp) != 2)
        return 0;

    return 1;
}

int FILE_WriteU8(FILE* fp, u8 in)
{
    int ret = fputc(in, fp);
    if (ret == EOF)
        return 0;

    return 1;
}

int FILE_ReadFile(char* fileName, void* buffer, uint size, int offset)
{
	FILE* stream;

    char buf[256];
    if (!strcmp(fileName, "BRIT.OOL") || !strcmp(fileName, "UNDER.OOL") || !strcmp(fileName, "SAVED.OOL") ||
        !strcmp(fileName, "SAVED.GAM"))
    {
        sprintf(buf, "SAVEGAME" PATH_SEP "%s", fileName);
        fileName = buf;
    }
    else if (!strcmp(fileName, "party.sav"))
    {
        sprintf(buf, "U4SAVE" PATH_SEP "%s", fileName);
        fileName = buf;
    }

	stream = FILE_Open(fileName, "rb");
	if (stream == 0)
	{
		return -1;
	}

	fseek(stream, offset, SEEK_SET);
	fread(buffer, 1, size, stream);
	fclose(stream);

	return 0;
}

int FILE_WriteFile(char* fileName, void* buffer, uint size, int offset)
{
    FILE* stream;

    char buf[256];
    if (!strcmp(fileName, "BRIT.OOL") || !strcmp(fileName, "UNDER.OOL") || !strcmp(fileName, "SAVED.OOL") ||
        !strcmp(fileName, "SAVED.GAM"))
    {
#if defined(ENABLE_BUG_FIX)
        // BUGFIX: the SAVEGAME directory is never created, so writes here silently fail
        u5_mkdir("SAVEGAME");
#endif
        sprintf(buf, "SAVEGAME" PATH_SEP "%s", fileName);
        fileName = buf;
    }

    stream = FILE_Open(fileName, "wb");
    if (stream == 0)
    {
        return -1;
    }

    fseek(stream, offset, SEEK_SET);
    fwrite(buffer, size, 1, stream);
    fclose(stream);

    return 0;
}
