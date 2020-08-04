/*
#include <features.h>
#include "libc.h"

#define START "_start"

#include "crt_arch.h"

int main();
weak void _init();
weak void _fini();
int __libc_start_main(int (*)(), int, char **,
	void (*)(), void(*)(), void(*)());

void _start_c(long *p)
{
	int argc = p[0];
	char **argv = (void *)(p+1);
	__libc_start_main(main, argc, argv, _init, _fini, 0);
}
*/

extern void exit(int code);
extern int main (int argc, char **argv);

#include "syscall.h"
#include "unistd.h"
#include "fcntl.h"
#include "stdio.h"
#include "string.h"
#include "pthread_impl.h"
 
extern void exit(int code);
extern int main (int argc, char **argv);

#define START "_start"

#include "crt_arch.h"

char **environ; // pointer to array of char * strings that define the current environment variables

//The defines below are from kernel the header and they must be the same.
#define	USER_OFFSET 		0x40000000
#define	USER_STACK 			0xF0000000
#define	SIZE_2MB     		0x200000 //2MB

void __init_libc_soso(char **envp, char *pn);

struct pthread mainThread;


void _start_c(long *p)
{
    int argc = 0;
    char** argv = NULL;

    //char** const argvenv = (char**)(USER_STACK - SIZE_2MB);
    char** const argvenv = (char**)(USER_STACK);

    int i = 0;
    const char* a = argvenv[0];

    argv = argvenv;

    //args
    while (NULL != a)
    {
        a = argvenv[++i];
    }

    argc = i;

    ++i;

    a = &argvenv[i];

    environ = a;

    //__syscall2(37, "mainThread:%x\n", &mainThread);
    memset(&mainThread, 0, sizeof(struct pthread));


    //__syscall2(37, "environ:%x\n", environ);

    char* arg0 = argv[0];

    //__syscall2(37, "arg0:%s\n", arg0);

    //__syscall2(37, "__init_libc_soso:%x\n", __init_libc_soso);

    __init_libc_soso(environ, arg0);

    //__syscall2(37, "tid:%d\n", mainThread.tid);


    int returnValue = main(argc, argv);

    exit(returnValue);
}