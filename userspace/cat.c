#include <stdio.h>
#include <stdlib.h>

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

        setvbuf ( stdout , NULL , _IONBF , 0);

        printf("trying to read %d bytes blocks\n", n);

        FILE* f = fopen(file, "r");
        if (f)
        {
            char buffer[1024];

            setvbuf ( f , NULL , _IONBF , 0);

            int bytes = 0;
            do
            {
                bytes = fread(buffer, 1, n, f);
                //printf("bytes:%d\n", bytes);
                for (int i = 0; i < bytes; ++i)
                {
                    //printf("buffer[%d]=%d\n", i, buffer[i]);
                    putchar(buffer[i]);
                }

            } while (bytes > 0);

            fclose(f);
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
