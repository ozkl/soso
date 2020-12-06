#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    char* xx = (char*)0xBAD;
    

    if (argc > 1)
    {
        printf("Let's try to write to %p\n", xx);
        *xx = 5;
    }
    else
    {
        printf("Let's try to read from %p\n", xx);
        printf("Value: %d\n", *xx);
    }
    
    
    printf("Finished. Should not reach here!\n");

    return 0;
}
