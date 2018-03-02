#include "list.h"
#include "pipe.h"
#include "fs.h"
#include "alloc.h"
#include "fifobuffer.h"

static List* gPipeList = NULL;

static FileSystemNode* gPipesRoot = NULL;

static FileSystemDirent gDirent;

typedef struct Pipe
{
    char name[32];
    FifoBuffer* buffer;
    FileSystemNode* fsNode;
} Pipe;

static BOOL pipes_open(File *file, uint32 flags);
static FileSystemDirent *pipes_readdir(FileSystemNode *node, uint32 index);
static FileSystemNode *pipes_finddir(FileSystemNode *node, char *name);

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

        gPipesRoot->open = pipes_open;
        gPipesRoot->finddir = pipes_finddir;
        gPipesRoot->readdir = pipes_readdir;
    }
}

static BOOL pipes_open(File *file, uint32 flags)
{
    return TRUE;
}

static FileSystemDirent *pipes_readdir(FileSystemNode *node, uint32 index)
{
    int counter = 0;

    List_Foreach (n, gPipeList)
    {
        Pipe* p = (Pipe*)n->data;

        if (counter == index)
        {
            strcpy(gDirent.name, p->name);
            gDirent.fileType = FT_Pipe;

            return &gDirent;
        }
        ++counter;
    }

    return NULL;
}

static FileSystemNode *pipes_finddir(FileSystemNode *node, char *name)
{
    List_Foreach (n, gPipeList)
    {
        Pipe* p = (Pipe*)n->data;

        if (strcmp(name, p->name) == 0)
        {
            return p->fsNode;
        }
    }

    return NULL;
}

static int32 pipe_read(File *file, uint32 size, uint8 *buffer)
{
    if (0 == size || NULL == buffer)
    {
        return -1;
    }

    Pipe* pipe = file->node->privateNodeData;

    uint32 used = FifoBuffer_getSize(pipe->buffer);
    if (size > used)
    {
        //Block

        //TODO: implement with process state
        while ((used = FifoBuffer_getSize(pipe->buffer)) < size)
        {
            halt();
        }
    }

    return FifoBuffer_dequeue(pipe->buffer, buffer, size);
}

static int32 pipe_write(File *file, uint32 size, uint8 *buffer)
{
    if (0 == size || NULL == buffer)
    {
        return -1;
    }

    Pipe* pipe = file->node->privateNodeData;

    uint32 free = FifoBuffer_getFree(pipe->buffer);
    if (size > free)
    {
        //Block

        //TODO: implement with process state
        while ((free = FifoBuffer_getFree(pipe->buffer)) < size)
        {
            halt();
        }
    }

    return FifoBuffer_enqueue(pipe->buffer, buffer, size);
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

    pipe->fsNode = (FileSystemNode*)kmalloc(sizeof(FileSystemNode));
    memset((uint8*)pipe->fsNode, 0, sizeof(FileSystemNode));
    pipe->fsNode->privateNodeData = pipe;
    pipe->fsNode->open = pipes_open;
    pipe->fsNode->read = pipe_read;
    pipe->fsNode->write = pipe_write;

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
            kfree(p->fsNode);
            kfree(p);

            return TRUE;
        }
    }

    return FALSE;
}

BOOL existsPipe(const char* name)
{
    List_Foreach (n, gPipeList)
    {
        Pipe* p = (Pipe*)n->data;
        if (strcmp(name, p->name) == 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}
