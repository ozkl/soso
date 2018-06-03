#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        const char* file = argv[1];

        int n = 1024;

        if (argc > 2)
        {
            n = atoi(argv[2]);
        }


        int f = open(file, 0);
        if (f)
        {
            char buffer[1024];

            int bytes = 0;
            do
            {
                bytes = read(f, buffer, n);
                //printf("bytes:%d\n", bytes);
                /*
                for (int i = 0; i < bytes; ++i)
                {
                    putchar(buffer[i]);
                }
                */
                write(1, buffer, bytes);

            } while (bytes > 0);

            close(f);
        }
        else
        {
            printf("Could not open for read: %s\n", file);
        }
    }
    else
    {
        printf("no input!\n");
    }

    return 0;
}
