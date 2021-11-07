#ifndef __UTIL_H__
#define __UTIL_H__

void debugPrint(const char *str);
void debugPrint(String str);
void debugPrintln(const char *str);
void debugPrintln(String str);
int debugPrintf(const char *format, ...);

#define HEXDUMP_COLS 16

void hexdump(void *mem, unsigned int len);

#endif // __UTIL_H__
