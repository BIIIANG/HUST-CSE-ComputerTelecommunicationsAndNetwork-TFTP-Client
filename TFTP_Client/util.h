#ifndef UTIL_H
#define UTIL_H

#include "def.h"

#define LF '\n'
#define CR '\r'
#define NUL '\0'
#define PLATFORM_WINDOWS 1
#define PLATFORM_LINUX 2

#define FULLSTRLEN(str) (strlen(str) + 1)
#define FULLNUMLEN(num) (numlen(num) + 1)

int appendMsg(char* buf, rsize_t bufferCount, int* offset, const char* format, ...);
int getMsg(char* buf, rsize_t bufferCount, const char* source, int* offset);
int getFileSize(const char* fileName);
int numlen(int num);

int encodeNetascii(const char *fileName);
int decodeNetascii(const char *fileName, int platform);

#endif // UTIL_H
