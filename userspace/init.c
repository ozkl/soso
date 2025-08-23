#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/syscall.h>

#define VT_ACTIVATE	    0x5606
#define VT_SOSO_DISABLE 0x9902f

int execute_on_tty(const char *path, char *const argv[], char *const envp[], const char *tty_path)
{
	return syscall(3008, path, argv, envp, tty_path);
}

int main()
{
    char* envp[] = {"HOME=/", "PATH=/initrd", NULL};

    char* argv[2];
    argv[0] = "/initrd/shell";
    argv[1] = NULL;

    execute_on_tty(argv[0], argv, envp, "/dev/pts/1");
    execute_on_tty(argv[0], argv, envp, "/dev/pts/2");
    execute_on_tty(argv[0], argv, envp, "/dev/pts/3");
    execute_on_tty(argv[0], argv, envp, "/dev/pts/4");

    int console_fd = open("/dev/console", 0, 0);
    if (console_fd >= 0)
    {
        ioctl(console_fd, VT_SOSO_DISABLE, 7);
        ioctl(console_fd, VT_ACTIVATE, 7);
    }

    argv[0] = "/initrd/nano-X";
    execute_on_tty(argv[0], argv, envp, "/dev/pts/7");

    usleep(1000 * 500);

    argv[0] = "/initrd/nanowm";
    execute_on_tty(argv[0], argv, envp, "/dev/null");

    usleep(1000 * 500);

    argv[0] = "/initrd/term";
    execute_on_tty(argv[0], argv, envp, "/dev/null");

    while (1)
    {
        usleep(1000 * 1000);
    }
    
    return 0;
}
