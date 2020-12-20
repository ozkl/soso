#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include "syscall.h"

int posix_openpt(int flags)
{
	//int r = open("/dev/ptmx", flags);
	//if (r < 0 && errno == ENOSPC) errno = EAGAIN;
	//return r;
	return __syscall(SYS_posix_openpt, flags);
}

int grantpt(int fd)
{
	return 0;
}

int unlockpt(int fd)
{
	int unlock = 0;
	return ioctl(fd, TIOCSPTLCK, &unlock);
}

int __ptsname_r(int fd, char *buf, size_t len)
{
	/* int pty, err;
	if (!buf) len = 0;
	if ((err = __syscall(SYS_ioctl, fd, TIOCGPTN, &pty))) return -err;
	if (snprintf(buf, len, "/dev/pts/%d", pty) >= len) return ERANGE;
	return 0; */
	return __syscall(SYS_ptsname_r, fd, buf, len);
}

weak_alias(__ptsname_r, ptsname_r);
