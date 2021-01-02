#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/select.h>
#include <sys/time.h>


#define BUFFER_SIZE 128

int main()
{
    int fdSerial = open("/dev/com1", O_RDWR);

    if (fdSerial < 0)
    {
        return 1;
    }

    fd_set rfds;
    struct timeval tv;

    char buffer[BUFFER_SIZE];

    while (1)
    {
        FD_ZERO(&rfds);
        FD_SET(fdSerial, &rfds);
        /* Wait up to five seconds. */
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        int sel = select(fdSerial + 1, &rfds, NULL, NULL, &tv);
        printf("select returned %d and flag:%d\n", sel, FD_ISSET(fdSerial, &rfds));

        if (sel > 0 && FD_ISSET(fdSerial, &rfds))
        {
            int bytes = read(fdSerial, buffer, BUFFER_SIZE);

            printf("%d bytes read\n", bytes);
        }
        else
        {
            printf("no reading\n");
        }
    }
    

    return 0;
}