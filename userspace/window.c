#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sosousdk.h>

int main(int argc, char** argv)
{
    unsigned int handle = createWindow(500, 300);
    setWindowPosition(handle, 400, 400);

    char* buffer = malloc(500 * 300 * 4);
    memset(buffer, 255, 500 * 300 * 4);

    char text[64];
    int i = 0;
    int line = 0;
    while (1)
    {
        ++i;
        sprintf(text, "%d : %d", i, getUptimeMilliseconds());
        for (int index = 0; text[index] != 0; ++index)
        {
            drawCharAt(buffer, text[index], index, line, 500, 300, 0, 0xFFFFFFFF);
        }
        copyToWindowBuffer(handle, buffer);

        if (i % 100 == 0)
        {
            ++line;
        }
    }

    return 0;
}
