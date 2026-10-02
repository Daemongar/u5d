#ifndef _COMMON_FILE_H
#define _COMMON_FILE_H

#include <stdio.h>

// fopen that tolerates case mismatches between requested and on-disk names.
FILE* FILE_Open(const char* path, const char* mode);
void FILE_ResolvePath(const char* path, char* out, int outSize);

int FILE_ReadU32LE(FILE* fp, u32* out);
int FILE_ReadU16LE(FILE* fp, u16* out);
int FILE_ReadU8(FILE* fp, u8* out);

int FILE_WriteU32LE(FILE* fp, u32 in);
int FILE_WriteU16LE(FILE* fp, u16 in);
int FILE_WriteU8(FILE* fp, u8 in);

int FILE_ReadFile(char* fileName, void* buffer, uint size, int offset);
int FILE_WriteFile(char* fileName, void* buffer, uint size, int offset);

#endif
