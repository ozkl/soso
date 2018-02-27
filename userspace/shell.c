#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>

typedef enum FileType
{
    FT_File               = 1,
    FT_Directory          = 2,
    FT_CharacterDevice    = 3,
    FT_BlockDevice        = 4,
    FT_Pipe               = 5,
    FT_SymbolicLink       = 6,
    FT_MountPoint         = 8
} FileType;

typedef struct FileSystemDirent
{
    char name[128];
    FileType fileType;
    int inode;
} FileSystemDirent;

int getdents(int fd, char *buf, int nbytes);
int execute(const char *path, char *const argv[], char *const envp[]);
int executep(const char *filename, char *const argv[], char *const envp[]);
int getWorkingDirectory(char *buf, int size);
int setWorkingDirectory(const char *path);

static void listFs2(const char* path)
{
    //Screen_PrintF("listFs2 - 1\n");

    int fd = open(path, 0);
    //printf("open(%s):%d\n", path, fd);
    if (fd < 0)
    {
        return;
    }

    //Screen_PrintF("listFs2 - 2\n");

    char buffer[1024];
    int bytesRead = getdents(fd, buffer, 1024);
    //printf("getdents():%d\n", bytesRead);
    if (bytesRead < 0)
    {
        close(fd);
        return;
    }

    //Screen_PrintF("listFs2 - 3\n");

    char pathBuffer[128];


    int byteCounter = 0;

    while (byteCounter + sizeof(FileSystemDirent) <= bytesRead)
    {
        FileSystemDirent *dirEntry = (FileSystemDirent*)(buffer + byteCounter);

        if ((dirEntry->fileType & FT_MountPoint) == FT_MountPoint)
        {
            printf("m");
        }
        else if (dirEntry->fileType == FT_File)
        {
            printf("f");
        }
        else if (dirEntry->fileType == FT_Directory)
        {
            printf("d");
        }
        else if (dirEntry->fileType == FT_BlockDevice)
        {
            printf("b");
        }
        else if (dirEntry->fileType == FT_CharacterDevice)
        {
            printf("c");
        }
        else if (dirEntry->fileType == FT_Pipe)
        {
            printf("p");
        }
        else if (dirEntry->fileType == FT_SymbolicLink)
        {
            printf("l");
        }
        else
        {
            printf("*");
        }


        sprintf(pathBuffer, "%s/%s", path, dirEntry->name);
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

        printf(" %s", dirEntry->name);

        if ((dirEntry->fileType & FT_Directory) == FT_Directory)
        {
            printf("/");
        }

        printf("\n");

        byteCounter += sizeof(FileSystemDirent);
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

int main(int argc, char **argv)
{
    char bufferIn[300];
    char cwd[128];

    printf("User shell v0.3 (pid:%d)!\n", getpid());

    while (1)
    {
        int result = getWorkingDirectory(cwd, 128);

        if (result >= 0)
        {
            printf("[%s]", cwd);
        }
        printf("$");
        fflush(stdout);

        fgets(bufferIn, 300, stdin);

        //printf("[%s]\n", bufferIn);

        int len = strlen(bufferIn);
        //printf("len:%d\n", len);
        if (bufferIn[len-1] == '\n')
        {
            bufferIn[len-1] = '\0';
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
            int result = execute(exe, argArray, environ);

            if (result >= 0)
            {
                printf("Started pid:%d\n", result);
                fflush(stdout);

                int waitStatus = 0;
                wait(&waitStatus);

                printf("Exited pid:%d\n", result);
                fflush(stdout);
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

                int waitStatus = 0;
                wait(&waitStatus);

                printf("Exited pid:%d\n", result);
                fflush(stdout);
            }
            else
            {
                if (strncmp(bufferIn, "kill", 4) == 0)
                {
                    int number = 0;
                    sscanf(bufferIn, "kill %d", &number);

                    printf("Killing:%d\n", number);
                    fflush(stdout);

                    kill(number, 0);
                }
                if (strncmp(bufferIn, "fork", 4) == 0)
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

                    listFs2(path);
                }
                else if (strncmp(bufferIn, "cd", 2) == 0)
                {
                    const char* path = bufferIn + 3;

                    if (setWorkingDirectory(path) < 0)
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
            }
        }

        destroyArray(argArray);
    }

    return 0;
}
