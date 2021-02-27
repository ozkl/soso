#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termios.h>

#include <soso.h>


static void listDirectory(const char* path)
{
    int fd = open(path, 0);

    if (fd < 0)
    {
        return;
    }


    FileSystemDirent dirEntry;
    memset(&dirEntry, 0, sizeof(FileSystemDirent));

    int index = 0;
    while (soso_read_dir(fd, &dirEntry, index++) != -1)
    {
        char pathBuffer[128];

        if ((dirEntry.fileType & FT_MountPoint) == FT_MountPoint)
        {
            printf("m");
        }
        else if (dirEntry.fileType == FT_File)
        {
            printf("f");
        }
        else if (dirEntry.fileType == FT_Directory)
        {
            printf("d");
        }
        else if (dirEntry.fileType == FT_BlockDevice)
        {
            printf("b");
        }
        else if (dirEntry.fileType == FT_CharacterDevice)
        {
            printf("c");
        }
        else if (dirEntry.fileType == FT_Pipe)
        {
            printf("p");
        }
        else if (dirEntry.fileType == FT_SymbolicLink)
        {
            printf("l");
        }
        else
        {
            printf("*");
        }


        sprintf(pathBuffer, "%s/%s", path, dirEntry.name);
        struct stat s;
        memset(&s, 0, sizeof(s));
        if (stat(pathBuffer, &s) != -1)
        {
            printf(" %10u", (unsigned int)s.st_size);
        }
        else
        {
            printf(" %10s", "");
        }

        printf(" %s", dirEntry.name);

        if ((dirEntry.fileType & FT_Directory) == FT_Directory)
        {
            printf("/");
        }

        printf("\n");
    }

    close(fd);
}

extern char **environ;

static void printEnvironment()
{
    int i = 1;
    char *s = *environ;

    for (; s; i++)
    {
        printf("%s\n", s);
        s = *(environ+i);
    }
}

static int getArrayItemCount(char *const array[])
{
    if (NULL == array)
    {
        return 0;
    }

    int i = 0;
    const char* a = array[0];
    while (NULL != a)
    {
        a = array[++i];
    }

    return i;
}

static char** createArray(const char* cmdline)
{
    //TODO: make 50 below n
    char** array = malloc(sizeof(char*)*50);
    int i = 0;

    const char* bb = cmdline;

    char single[128];

    memset(array, 0, sizeof(char*)*50);

    while (bb != NULL)
    {
        const char* token = strstr(bb, " ");

        if (NULL == token)
        {
            int l = strlen(bb);
            if (l > 0)
            {
                token = bb + l;
            }
        }

        if (token)
        {
            int len = token - bb;

            if (len > 0)
            {
                strncpy(single, bb, len);
                single[len] = '\0';

                //printf("%d:[%s]\n", len, single);

                char* str = malloc(len + 1);
                strcpy(str, single);

                array[i++] = str;
            }

            bb = token + 1;
        }
        else
        {
            break;
        }
    }

    return array;
}

static void destroyArray(char** array)
{
    char* a = array[0];

    int i = 0;

    while (NULL != a)
    {
        free(a);

        a = array[++i];
    }

    free(array);
}

static void handleKill(const char* bufferIn)
{
    int number1 = 0;
    int number2 = 0;
    int scanned = sscanf(bufferIn, "kill %d %d", &number1, &number2);

    int pid = number1;
    int signal = SIGTERM;

    if (scanned == 1)
    {
        pid = number1;
    }
    else if (scanned == 2)
    {
        signal = number1;
        pid = number2;
    }
    else
    {
        printf("usage: kill [signal] pid\n");
        fflush(stdout);
        return;
    }
    

    printf("Signal %d to pid %d\n", signal, pid);
    fflush(stdout);

    if (kill(pid, signal) < 0)
    {
        printf("kill(%d, %d) failed!\n", pid, signal);
    }
    else
    {
        printf("kill(%d, %d) success!\n", pid, signal);
    }

    fflush(stdout);
}

int main(int argc, char **argv)
{
    char bufferIn[300];
    char cwd[128];

    int shellPid = getpid();

    printf("User shell v0.4 (pid:%d)!\n", shellPid);
    //printf("sizeof(size_t)=%d\n", sizeof(size_t));

    while (1)
    {
        char* result = getcwd(cwd, 128);

        if (result == cwd)
        {
            printf("[%s]", cwd);
        }
        printf("$");
        fflush(stdout);

        //sleepMilliseconds(500);

        char* r = fgets(bufferIn, 300, stdin);

        if (r == NULL)
        {
            continue;
        }

        //printf("[%s]\n", bufferIn);

        int background = 0;

        int len = strlen(bufferIn);
        //printf("len:%d\n", len);
        if (bufferIn[len-1] == '\n')
        {
            bufferIn[len-1] = '\0';
        }

        if (bufferIn[len-2] == '&')
        {
            background = 1;
            bufferIn[len-2] = '\0';
        }

        //printf("[%s]\n", bufferIn);
        //sscanf(buffer, "%d", &number);

        char** argArray = createArray(bufferIn);
        int argArrayCount = getArrayItemCount(argArray);

        const char* exe = bufferIn;
        if (argArrayCount > 0)
        {
            exe = argArray[0];
        }

        //printf("exe:[%s] count:%d\n", exe, argArrayCount);
        //fflush(stdout);

        if (bufferIn[0] == '/')
        {
            //trying to execute something
            int result = 0;
            if (background)
            {
                result = execute_on_tty(exe, argArray, environ, "/dev/null");
            }
            else
            {
                result = execute(exe, argArray, environ);
            }


            if (result >= 0)
            {
                printf("Started pid:%d\n", result);
                fflush(stdout);

                if (background == 0)
                {
                    tcsetpgrp(0, result);
                    int waitStatus = 0;
                    wait(&waitStatus);

                    tcsetpgrp(0, shellPid);

                    printf("Exited pid:%d\n", result);
                    fflush(stdout);
                }
            }
            else
            {
                printf("Could not execute:%s\n", bufferIn);
                fflush(stdout);
            }
        }
        else
        {

            int result = executep(exe, argArray, environ);
            if (result >= 0)
            {
                
                printf("Started pid:%d\n", result);
                fflush(stdout);

                if (background == 0)
                {
                    tcsetpgrp(0, result);
                    int waitStatus = 0;
                    wait(&waitStatus);

                    tcsetpgrp(0, shellPid);

                    printf("Exited pid:%d\n", result);
                    fflush(stdout);
                }
            }
            else
            {
                if (strncmp(bufferIn, "kill", 4) == 0)
                {
                    handleKill(bufferIn);
                }
                else if (strncmp(bufferIn, "fork", 4) == 0)
                {
                    int number = fork();

                    if (number == 0)
                    {
                        printf("fork I am zero and getpid:%d\n", getpid());
                        fflush(stdout);
                    }
                    else
                    {
                        printf("fork I am %d and getpid:%d\n", number, getpid());
                        fflush(stdout);
                    }
                }
                else if (strncmp(bufferIn, "ls", 2) == 0)
                {
                    char* path = bufferIn + 3;

                    if (strlen(path) == 0)
                    {
                        path = cwd;
                    }

                    listDirectory(path);
                }
                else if (strncmp(bufferIn, "cd", 2) == 0)
                {
                    const char* path = bufferIn + 3;

                    if (chdir(path) < 0)
                    {
                        printf("Directory unavailable:%s\n", path);
                        fflush(stdout);
                    }
                }
                else if (strncmp(bufferIn, "env", 3) == 0)
                {
                    printEnvironment();
                    fflush(stdout);
                }
                else if (strncmp(bufferIn, "putenv", 6) == 0)
                {
                    char* e = bufferIn + 7;

                    putenv(e);
                }
                else if (strncmp(bufferIn, "exit", 4) == 0)
                {
                    printf("Exiting\n");
                    fflush(stdout);

                    return 0;
                }
                else if (strncmp(bufferIn, "off", 3) == 0)
                {
                    struct termios raw;
                    tcgetattr(STDIN_FILENO, &raw);
                    raw.c_lflag &= ~(ECHO);
                    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
                }
                else if (strncmp(bufferIn, "on", 3) == 0)
                {
                    struct termios raw;
                    tcgetattr(STDIN_FILENO, &raw);
                    raw.c_lflag |= ECHO;
                    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
                }
                else
                {
                    printf("Could not execute:%s\n", bufferIn);
                    fflush(stdout);
                }
            }
        }

        destroyArray(argArray);
    }

    return 0;
}
