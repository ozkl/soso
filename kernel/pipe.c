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
    List* readers;
    List* writers;
    BOOL isBroken;
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

static void blockAccessingThreads(Pipe* pipe, List* list)
{
    disableInterrupts();

    List_Foreach (n, list)
    {
        Thread* reader = n->data;

        changeThreadState(reader, TS_WAITIO, pipe);
    }

    enableInterrupts();

    halt();
}

static void wakeupAccessingThreads(Pipe* pipe, List* list)
{
    beginCriticalSection();

    List_Foreach (n, list)
    {
        Thread* reader = n->data;

        if (reader->state == TS_WAITIO)
        {
            if (reader->state_privateData == pipe)
            {
                resumeThread(reader);
            }
        }
    }

    endCriticalSection();
}

static BOOL pipe_open(File *file, uint32 flags)
{
    if (CHECK_ACCESS(file->flags, O_RDONLY) || CHECK_ACCESS(file->flags, O_WRONLY))
    {
        beginCriticalSection();

        gCurrentThread->state = TS_CRITICAL;

        Pipe* pipe = file->node->privateNodeData;

        pipe->isBroken = FALSE;

        if (CHECK_ACCESS(file->flags, O_RDONLY))
        {
            List_Append(pipe->readers, file->thread);

            wakeupAccessingThreads(pipe, pipe->writers);
        }
        else if (CHECK_ACCESS(file->flags, O_WRONLY))
        {
            List_Append(pipe->writers, file->thread);
        }

        gCurrentThread->state = TS_RUN;
        
        endCriticalSection();

        return TRUE;
    }

    return FALSE;
}

static void pipe_close(File *file)
{
    beginCriticalSection();

    Pipe* pipe = file->node->privateNodeData;

    if (CHECK_ACCESS(file->flags, O_RDONLY))
    {
        List_RemoveFirstOccurrence(pipe->readers, file->thread);
    }
    else if (CHECK_ACCESS(file->flags, O_WRONLY))
    {
        List_RemoveFirstOccurrence(pipe->writers, file->thread);
    }

    if (List_IsEmpty(pipe->readers))
    {
        //No readers left
        pipe->isBroken = TRUE;

        wakeupAccessingThreads(pipe, pipe->writers);
    }

    endCriticalSection();
}

static BOOL pipe_readTestReady(File *file)
{
    Pipe* pipe = file->node->privateNodeData;

    if (FifoBuffer_getSize(pipe->buffer) > 0)
    {
        return TRUE;
    }

    return FALSE;
}

static int32 pipe_read(File *file, uint32 size, uint8 *buffer)
{
    if (0 == size || NULL == buffer || !CHECK_ACCESS(file->flags, O_RDONLY))
    {
        return -1;
    }

    Pipe* pipe = file->node->privateNodeData;

    uint32 used = 0;
    while (pipe_readTestReady(file) == FALSE)
    {
        if (pipe->isBroken)
        {
            disableInterrupts();
            return -1;
        }

        blockAccessingThreads(pipe, pipe->readers);
    }

    disableInterrupts();



    int32 readBytes = FifoBuffer_dequeue(pipe->buffer, buffer, size);

    wakeupAccessingThreads(pipe, pipe->writers);

    return readBytes;
}

static BOOL pipe_writeTestReady(File *file)
{
    Pipe* pipe = file->node->privateNodeData;

    beginCriticalSection();
    int readerCount = List_GetCount(pipe->readers);
    endCriticalSection();

    if (FifoBuffer_getFree(pipe->buffer) > 0 && readerCount > 0)
    {
        return TRUE;
    }

    return FALSE;
}

static int32 pipe_write(File *file, uint32 size, uint8 *buffer)
{
    if (0 == size || NULL == buffer || !CHECK_ACCESS(file->flags, O_WRONLY))
    {
        return -1;
    }

    Pipe* pipe = file->node->privateNodeData;

    uint32 free = 0;
    while (pipe_writeTestReady(file) == FALSE)
    {
        if (pipe->isBroken)
        {
            disableInterrupts();
            return -1;
        }

        blockAccessingThreads(pipe, pipe->writers);
    }

    disableInterrupts();

    int32 bytesWritten = FifoBuffer_enqueue(pipe->buffer, buffer, size);

    wakeupAccessingThreads(pipe, pipe->readers);

    return bytesWritten;
}

BOOL createPipe(const char* name, uint32 bufferSize)
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

    pipe->isBroken = FALSE;

    strcpy(pipe->name, name);
    pipe->buffer = FifoBuffer_create(bufferSize);

    pipe->readers = List_Create();
    pipe->writers = List_Create();

    pipe->fsNode = (FileSystemNode*)kmalloc(sizeof(FileSystemNode));
    memset((uint8*)pipe->fsNode, 0, sizeof(FileSystemNode));
    pipe->fsNode->privateNodeData = pipe;
    pipe->fsNode->open = pipe_open;
    pipe->fsNode->close = pipe_close;
    pipe->fsNode->read = pipe_read;
    pipe->fsNode->write = pipe_write;
    pipe->fsNode->readTestReady = pipe_readTestReady;
    pipe->fsNode->writeTestReady = pipe_writeTestReady;

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
            List_Destroy(p->readers);
            List_Destroy(p->writers);
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
