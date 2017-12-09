#include <stdio.h>

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        const char* file = argv[1];

        FILE* f = fopen(file, "r");
        if (f)
        {
            char buffer[1024];

            int bytes = 0;
            do
            {
                bytes = fread(buffer, 1, 1024, f);

                for (int i = 0; i < bytes; ++i)
                {
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
