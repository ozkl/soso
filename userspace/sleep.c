#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

int main(int argc, char** argv)
{
    int seconds = 1;

    if (argc > 1)
    {
        sscanf(argv[1], "%d", &seconds);
    }

    printf("sleeping %d seconds\n", seconds);
    sleep(seconds);
    printf("Finished.\n");

    return 0;
}
