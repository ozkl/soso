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


        int f = open(file, O_RDONLY);
        if (f >= 0)
        {
            char buffer[1024];

            int bytes = 0;
            do
            {
                bytes = read(f, buffer, n);
                
                if (bytes < 0)
                {
                    write(1, "read negative!\n", bytes);
                    break;
                }
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
