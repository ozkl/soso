#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
    if (argc > 2)
    {
        const char* file = argv[1];
        const char* text = argv[2];

        FILE* f = fopen(file, "w");
        if (f)
        {
            int bytes = strlen(text);
            fwrite(text, 1, bytes, f);

            fclose(f);
        }
        else
        {
            printf("Could not open for write: %s\n", file);
        }
    }
    else
    {
        printf("no input!\n");
    }

    return 0;
}
