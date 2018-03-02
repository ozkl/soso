#include "list.h"
#include "pipe.h"
#include "fs.h"
#include "alloc.h"
#include "fifobuffer.h"

static List* gPipeList = NULL;

static FileSystemNode* gPipesRoot = NULL;

typedef struct Pipe
{
    char name[32];
    FifoBuffer* buffer;
} Pipe;

void initializePipes()
{
    gPipeList = List_Create();

    gPipesRoot = getFileSystemNode("/system/pipes");

    if (NULL == gPipesRoot)
    {
        WARNING("/system/pipes not found!!");
    }
    else
    {
        //TODO

        //gPipesRoot->open =
        //gPipesRoot->read =
        //gPipesRoot->write =
        //gPipesRoot->close =
    }
}

List* getPipeList()
{
    return gPipeList;
}

BOOL createPipe(const char* name)
{
    List_Foreach (n, gPipeList)
    {
        Pipe* p = (Pipe*)n->data;
        if (strcmp(name, p->name) == 0)
        {
            return FALSE;
        }
    }

    Pipe* pipe = (Pipe*)kmalloc(sizeof(Pipe));
    memset((uint8*)pipe, 0, sizeof(Pipe));

    strcpy(pipe->name, name);
    pipe->buffer = FifoBuffer_create(1024 * 1024);

    List_Append(gPipeList, pipe);

    return TRUE;
}

BOOL destroyPipe(const char* name)
{
    List_Foreach (n, gPipeList)
    {
        Pipe* p = (Pipe*)n->data;
        if (strcmp(name, p->name) == 0)
        {
            List_RemoveFirstOccurrence(gPipeList, p);
            FifoBuffer_destroy(p->buffer);
            kfree(p);

            return TRUE;
        }
    }

    return FALSE;
}
