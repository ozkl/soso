#include <fcntl.h>
#include <stdio.h>
 
extern void exit(int code);
extern int main (int argc, char **argv);

char **environ; /* pointer to array of char * strings that define the current environment variables */

//The defines below are from kernel the header and they must be the same.
#define	USER_OFFSET 		0x40000000
#define	USER_EXE_IMAGE 		0x200000 //2MB
#define	USER_ARGV_ENV_SIZE	0x10000  //65KB
#define	USER_ARGV_ENV_LOC	(USER_OFFSET + (USER_EXE_IMAGE - USER_ARGV_ENV_SIZE))
 
void _start()
{
    int argc = 0;
    char** argv = NULL;

    _init_signal();

    char** const argvenv = (char**)USER_ARGV_ENV_LOC;

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

    int returnValue = main(argc, argv);

    exit(returnValue);
}
