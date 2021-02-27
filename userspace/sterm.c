#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>

#include <soso.h>

#define BUFFER_SIZE 128

#define MAX(a,b) (((a)>(b))?(a):(b))

extern char **environ;

int main()
{
    int fdSerial = open("/dev/com1", O_RDWR);

    if (fdSerial < 0)
    {
        return 1;
    }

    int fdMaster = posix_openpt(0);
    //printf("master:%d\n", master);

    if (fdMaster < 0)
    {
        return 1;
    }

    char slavePath[128];
    ptsname_r(fdMaster, slavePath, 128);
    
    fd_set rfds;
    struct timeval tv;

    char* argv[2];
    argv[0] = "/initrd/shell";
    argv[1] = NULL;

    execute_on_tty(argv[0], argv, environ, slavePath);

    int maxFd = MAX(fdSerial, fdMaster);

    char buffer[BUFFER_SIZE];
    while (1)
    {
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        FD_ZERO(&rfds);
        FD_SET(fdSerial, &rfds);
        FD_SET(fdMaster, &rfds);

        int sel = select(maxFd + 1, &rfds, NULL, NULL, &tv);

        if (sel > 0)
        {
            if (FD_ISSET(fdSerial, &rfds))
            {
                int bytes = read(fdSerial, buffer, BUFFER_SIZE);

                if (bytes > 0)
                {
                    write(fdMaster, buffer, bytes);
                }
            }

            if (FD_ISSET(fdMaster, &rfds))
            {
                int bytes = read(fdMaster, buffer, BUFFER_SIZE);

                if (bytes > 0)
                {
                    write(fdSerial, buffer, bytes);
                }
            }
        }
    }
    

    return 0;
}