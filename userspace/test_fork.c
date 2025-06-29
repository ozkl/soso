#include <unistd.h>
#include <stdio.h>

int main(int argc, char** rgv)
{
    int pid = getpid();

    printf("I am parent:%d\n", pid);

    int child = fork();
    if (child == 0)
    {
        int child_pid = getpid();
        for (size_t i = 0; i < 5; i++)
        {
            printf("%d:I am child:%d\n", i, child_pid);
            sleep(1);
        }
    }
    else
    {
        int parent_pid = getpid();
        for (size_t i = 0; i < 5; i++)
        {
            printf("%d:I am parent:%d and my child:%d\n", i, parent_pid, child);
            sleep(1);
        }

        while (1)
        {
            sleep(1);
        }
    }
    return 0;
}