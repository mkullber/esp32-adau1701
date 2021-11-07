#include <Arduino.h>
#include <IotWebConf.h>

#include "util.h"

extern WiFiClient remoteClient;

void debugPrint(const char *str)
{
    Serial.print(str);

    if (remoteClient.connected())
    {
        remoteClient.write(str);
    }
}

void debugPrint(String str)
{
    debugPrint(str.c_str());
}

void debugPrintln(const char *str)
{
    debugPrint(str);
    debugPrint("\n");
}

void debugPrintln(String str)
{
    debugPrintln(str.c_str());
}

int debugPrintf(const char *format, ...)
{
    char debugBuf[256] = {0};
    // TODO: doesn't handle % characters
    va_list argptr;
    va_start(argptr, format);
    int written = vsnprintf(debugBuf, sizeof(debugBuf) - 1, format, argptr);
    va_end(argptr);

    debugPrint(debugBuf);
    return written;
}

void hexdump(void *mem, unsigned int len)
{
    unsigned int i, j;

    for (i = 0; i < len + ((len % HEXDUMP_COLS) ? (HEXDUMP_COLS - len % HEXDUMP_COLS) : 0); i++)
    {
        /* print offset */
        if (i % HEXDUMP_COLS == 0)
        {
            printf("0x%04x: ", i);
        }

        if (i % (HEXDUMP_COLS / 2) == 0 && i % HEXDUMP_COLS != 0)
        {
            putchar(' ');
        }

        /* print hex data */
        if (i < len)
        {
            printf("%02x ", 0xFF & ((char *)mem)[i]);
        }
        else /* end of block, just aligning for ASCII dump */
        {
            printf("   ");
        }

        /* print ASCII dump */
        if (i % HEXDUMP_COLS == (HEXDUMP_COLS - 1))
        {
            putchar(' ');
            for (j = i - (HEXDUMP_COLS - 1); j <= i; j++)
            {
                if (j % (HEXDUMP_COLS / 2) == 0 && j % HEXDUMP_COLS != 0)
                {
                    putchar(' ');
                }

                if (j >= len) /* end of block, not really printing */
                {
                    putchar(' ');
                }
                else if (isprint(((char *)mem)[j])) /* printable char */
                {
                    putchar(0xFF & ((char *)mem)[j]);
                }
                else /* other char */
                {
                    putchar('.');
                }
            }
            putchar('\n');
        }
    }
}
