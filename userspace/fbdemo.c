#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>

#include <sosousdk.h>

int main(int argc, char** argv)
{
    int fd = open("/dev/fb0", 0);
    if (fd >= 0)
    {
        int* buffer = mmap(NULL, 1024*768*4, 0, fd, 0);

        if (buffer != (int*)-1)
        {
            while (1)
            {
                for (int i = 0; i < 1024*768; ++i)
                {
                    buffer[i] = rand();
                }
            }
        }
        else
        {
            printf("mmap failed\n");
        }
    }
    else
    {
        printf("could not open /dev/fb0\n");
    }

    return 0;
}
