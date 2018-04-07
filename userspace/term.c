#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <sosousdk.h>

int executeOnTTY(const char *path, char *const argv[], char *const envp[], const char *ttyPath);

extern char **environ;

static char** createArray(const char* cmdline)
{
    //TODO: make 50 below n
    char** array = malloc(sizeof(char*)*50);
    int i = 0;

    const char* bb = cmdline;

    char single[128];

    memset(array, 0, sizeof(char*)*50);

    while (bb != NULL)
    {
        const char* token = strstr(bb, " ");

        if (NULL == token)
        {
            int l = strlen(bb);
            if (l > 0)
            {
                token = bb + l;
            }
        }

        if (token)
        {
            int len = token - bb;

            if (len > 0)
            {
                strncpy(single, bb, len);
                single[len] = '\0';

                //printf("%d:[%s]\n", len, single);

                char* str = malloc(len + 1);
                strcpy(str, single);

                array[i++] = str;
            }

            bb = token + 1;
        }
        else
        {
            break;
        }
    }

    return array;
}

int main(int argc, char** argv)
{
    int width = 80 * 9;
    int height = 25 * 16;

    unsigned int handle = createWindow(width, height);
    setWindowPosition(handle, 50, 40);

    char* windowBuffer = malloc(width * height * 4);
    memset(windowBuffer, 255, width * height * 4);

    char** argArray = createArray("shell");
    int argArrayCount = 1;

    //int result = executeOnTTY(argArray[0], argArray, environ, "/dev/pt0");

    char bufferIn[300];

    int ttyFd = open("/dev/pt0", 0);

    if (ttyFd < -1)
    {
        return 1;
    }

    int bufferSize = getTTYBufferSize(ttyFd);

    if (bufferSize < -1)
    {
        return 1;
    }

    TtyUserBuffer tty;
    memset(&tty, 0, sizeof(TtyUserBuffer));
    tty.buffer = malloc(bufferSize);

    int line = 0;
    //TODO: query OS message and take key input and write to tty
    //read and paint
    while (1)
    {
        SosoMessage message;
        if (getNextMessage(&message) >= 0)
        {
            if (message.messageType == 0) //key
            {
                char scancode = message.parameter1;
                //fwrite(&scancode, 1, 1, f);
                //printf("key press code: %d\n", scancode);
                sendCharacterToTTY(ttyFd, scancode);
            }
        }

        if (getTTYBuffer(ttyFd, &tty) < 0)
        {
            break;
        }

        for (unsigned short r = 0; r < tty.lineCount; ++r)
        {
            for (unsigned short c = 0; c < tty.columnCount; ++c)
            {
                unsigned char* ttyPos = tty.buffer + (r * tty.columnCount + c) * 2;

                unsigned char chr = ttyPos[0];
                unsigned char color = ttyPos[1];

                drawCharAt(windowBuffer, chr, c, r, width, height, 0, 0xFFFFFFFF);
            }
        }

        copyToWindowBuffer(handle, windowBuffer);

        /*
        fgets(bufferIn, 300, f);
        int len = strlen(bufferIn);
        if (bufferIn[len-1] == '\n')
        {
            bufferIn[len-1] = '\0';
        }

        for (int index = 0; bufferIn[index] != 0; ++index)
        {
            drawCharAt(windowBuffer, bufferIn[index], index, line, width, height, 0, 0xFFFFFFFF);
        }
        copyToWindowBuffer(handle, windowBuffer);
        */
    }

    free(tty.buffer);

    return 0;
}
