#ifndef _COMMON_CONFIG_H
#define _COMMON_CONFIG_H

int CONFIG_GetString(char* section, char* key, char* defaultValue, char* outValue, int size);
int CONFIG_GetInt(char* section, char* key, int defaultValue);

// Write a value to ultima5.ini, replacing an existing key or adding it (and
// its section) if missing.
void CONFIG_SetString(char* section, char* key, char* value);
void CONFIG_SetInt(char* section, char* key, int value);

#endif
