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

void CONFIG_SetString(char* section, char* key, char* value)
{
    WritePrivateProfileStringA(section, key, value, ".\\ultima5.ini");
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

#define CONFIG_MAX_LINES 512
#define CONFIG_LINE_SIZE 512

void CONFIG_SetString(char* section, char* key, char* value)
{
    static char lines[CONFIG_MAX_LINES][CONFIG_LINE_SIZE];
    char path[512];
    int count = 0;
    int sectionLine = -1; // header of the matching section
    int sectionEnd = -1;  // last line belonging to it
    int keyLine = -1;
    FILE* fp;

    FILE_ResolvePath("ultima5.ini", path, sizeof(path));

    fp = fopen(path, "r");
    if (fp != NULL)
    {
        while (count < CONFIG_MAX_LINES && fgets(lines[count], CONFIG_LINE_SIZE, fp) != NULL)
        {
            char work[CONFIG_LINE_SIZE];
            char* s;

            lines[count][strcspn(lines[count], "\r\n")] = '\0';
            strcpy(work, lines[count]);
            s = ConfigTrim(work);

            if (*s == '[')
            {
                char* close = strchr(s, ']');
                if (close != NULL)
                {
                    *close = '\0';
                    if (sectionLine >= 0 && sectionEnd < 0)
                        sectionEnd = count - 1;
                    if (sectionLine < 0 && strcasecmp(ConfigTrim(s + 1), section) == 0)
                        sectionLine = count;
                }
            }
            else if (sectionLine >= 0 && sectionEnd < 0 && keyLine < 0 && *s != ';' && *s != '#')
            {
                char* eq = strchr(s, '=');
                if (eq != NULL)
                {
                    *eq = '\0';
                    if (strcasecmp(ConfigTrim(s), key) == 0)
                        keyLine = count;
                }
            }

            count++;
        }
        fclose(fp);
    }

    if (sectionLine >= 0 && sectionEnd < 0)
        sectionEnd = count - 1;

    // A new key goes after the last non-blank line of its section.
    int insertAfter = -1;
    if (keyLine < 0 && sectionLine >= 0)
    {
        insertAfter = sectionEnd;
        while (insertAfter > sectionLine)
        {
            char work[CONFIG_LINE_SIZE];
            strcpy(work, lines[insertAfter]);
            if (*ConfigTrim(work) != '\0')
                break;
            insertAfter--;
        }
    }

    fp = fopen(path, "w");
    if (fp == NULL)
        return;

    for (int i = 0; i < count; i++)
    {
        if (i == keyLine)
        {
            fprintf(fp, "%s=%s\n", key, value);
            continue;
        }

        fprintf(fp, "%s\n", lines[i]);
        if (i == insertAfter)
            fprintf(fp, "%s=%s\n", key, value);
    }

    if (sectionLine < 0)
        fprintf(fp, "%s[%s]\n%s=%s\n", count > 0 ? "\n" : "", section, key, value);

    fclose(fp);
}

#endif

void CONFIG_SetInt(char* section, char* key, int value)
{
    char text[32];
    sprintf(text, "%d", value);
    CONFIG_SetString(section, key, text);
}

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
