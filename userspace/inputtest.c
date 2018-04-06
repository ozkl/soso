#include <stdio.h>

int main()
{
    int n = 0;
    while (1)
    {
        printf("enter number (-1 to quit):");
        fflush(stdout);
        scanf("%d", &n);
        printf("you entered:%d\n", n);

        if (n == -1)
        {
            break;
        }
    }
    return 0;
}
