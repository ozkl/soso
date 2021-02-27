#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "syscall.h"
#include "soso.h"

int32_t execute_on_tty(const char *path, char *const argv[], char *const envp[], const char *ttyPath)
{
    return __syscall(SYS_execute_on_tty, path, argv, envp, ttyPath);
}

int32_t execute(const char *path, char *const argv[], char *const envp[])
{
    return __syscall(SYS_execute, path, argv, envp);
}

int32_t executep(const char *filename, char *const argv[], char *const envp[])
{
    const char* path = getenv("PATH");

    if (path)
    {
        const char* path2 = path;

        char* bb = path2;

        char single[128];
        while (bb != NULL)
        {
            char* token = strstr(bb, ":");

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

                    strcat(single, "/");
                    strcat(single, filename);

                    int result = execute(single, argv, envp);

                    if (result > 0)
                    {
                        return result;
                    }
                }

                bb = token + 1;
            }
            else
            {
                break;
            }
        }
    }

    return -1;
}