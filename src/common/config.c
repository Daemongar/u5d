#include "common.h"
#include "config.h"

#include <stdio.h>
#include <string.h>

// TODO

#if defined(OS_WINDOWS)

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

int CONFIG_GetString(char* section, char* key, char* defaultValue, char* outValue, int size)
{
    char fileName[] = ".\\ultima5.ini";

    return GetPrivateProfileStringA(section, key, defaultValue, outValue, size, fileName);
}

#else

#include <ctype.h>
#include <strings.h>

#include "file.h"

static char* ConfigTrim(char* s)
{
    char* end;

    while (isspace((unsigned char)*s))
        s++;

    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1]))
        *--end = '\0';

    return s;
}

// Reads ultima5.ini from the game directory, like GetPrivateProfileString on
// Windows: [section] headers, key=value lines, ';' or '#' comments, with
// section and key names matched case-insensitively.
int CONFIG_GetString(char* section, char* key, char* defaultValue, char* outValue, int size)
{
    char line[512];
    bool inSection = false;
    FILE* fp;

    if (defaultValue == NULL)
        defaultValue = "";

    strncpy(outValue, defaultValue, size - 1);
    outValue[size - 1] = '\0';

    fp = FILE_Open("ultima5.ini", "r");
    if (fp == NULL)
        return (int)strlen(outValue);

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char* s = ConfigTrim(line);

        if (*s == '\0' || *s == ';' || *s == '#')
            continue;

        if (*s == '[')
        {
            char* close = strchr(s, ']');
            if (close != NULL)
            {
                *close = '\0';
                inSection = strcasecmp(ConfigTrim(s + 1), section) == 0;
            }
            continue;
        }

        char* eq = strchr(s, '=');
        if (!inSection || eq == NULL)
            continue;

        *eq = '\0';
        if (strcasecmp(ConfigTrim(s), key) == 0)
        {
            strncpy(outValue, ConfigTrim(eq + 1), size - 1);
            outValue[size - 1] = '\0';
            break;
        }
    }

    fclose(fp);
    return (int)strlen(outValue);
}

#endif

int CONFIG_GetInt(char* section, char* key, int defaultValue)
{
    char defaultStr[32];
    sprintf(defaultStr, "%d", defaultValue);

    char outValue[256] = { 0, };
    CONFIG_GetString(section, key, defaultStr, outValue, 255);

    char* end;
    long val = strtol(outValue, &end, 10);
    if (end == outValue)
    {
        return defaultValue;
    }

    return (int)val;
}
