#include "list.h"
#include "pipe.h"
#include "fs.h"
#include "alloc.h"
#include "fifobuffer.h"
#include "errno.h"

static List* g_pipe_list = NULL;

static FileSystemNode* g_pipes_root = NULL;

static FileSystemDirent g_dirent;

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

void pipe_initialize()
{
    g_pipe_list = List_Create();

    g_pipes_root = fs_get_node("/system/pipes");

    if (NULL == g_pipes_root)
    {
        WARNING("/system/pipes not found!!");
    }
    else
    {
        g_pipes_root->open = pipes_open;
        g_pipes_root->finddir = pipes_finddir;
        g_pipes_root->readdir = pipes_readdir;
    }
}

static BOOL pipes_open(File *file, uint32 flags)
{
    return TRUE;
}

static FileSystemDirent *pipes_readdir(FileSystemNode *node, uint32 index)
{
    int counter = 0;

    List_Foreach (n, g_pipe_list)
    {
        Pipe* p = (Pipe*)n->data;

        if (counter == index)
        {
            strcpy(g_dirent.name, p->name);
            g_dirent.fileType = FT_Pipe;

            return &g_dirent;
        }
        ++counter;
    }

    return NULL;
}

static FileSystemNode *pipes_finddir(FileSystemNode *node, char *name)
{
    List_Foreach (n, g_pipe_list)
    {
        Pipe* p = (Pipe*)n->data;

        if (strcmp(name, p->name) == 0)
        {
            return p->fsNode;
        }
    }

    return NULL;
}

static void block_accessing_threads(Pipe* pipe, List* list)
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

static void wakeup_accessing_threads(Pipe* pipe, List* list)
{
    begin_critical_section();

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

    end_critical_section();
}

static BOOL pipe_open(File *file, uint32 flags)
{
    if (CHECK_ACCESS(file->flags, O_RDONLY) || CHECK_ACCESS(file->flags, O_WRONLY))
    {
        begin_critical_section();

        gCurrentThread->state = TS_CRITICAL;

        Pipe* pipe = file->node->privateNodeData;

        pipe->isBroken = FALSE;

        if (CHECK_ACCESS(file->flags, O_RDONLY))
        {
            List_Append(pipe->readers, file->thread);

            wakeup_accessing_threads(pipe, pipe->writers);
        }
        else if (CHECK_ACCESS(file->flags, O_WRONLY))
        {
            List_Append(pipe->writers, file->thread);
        }

        gCurrentThread->state = TS_RUN;

        end_critical_section();

        return TRUE;
    }

    return FALSE;
}

static void pipe_close(File *file)
{
    begin_critical_section();

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

        wakeup_accessing_threads(pipe, pipe->writers);
    }

    end_critical_section();
}

static BOOL pipe_read_test_ready(File *file)
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
    while (pipe_read_test_ready(file) == FALSE)
    {
        if (pipe->isBroken)
        {
            disableInterrupts();
            return -EPIPE;
        }

        if (gCurrentThread->pendingSignalCount > 0)
        {
            return -EINTR;
        }

        block_accessing_threads(pipe, pipe->readers);
    }

    if (gCurrentThread->pendingSignalCount > 0)
    {
        return -EINTR;
    }

    disableInterrupts();



    int32 readBytes = FifoBuffer_dequeue(pipe->buffer, buffer, size);

    wakeup_accessing_threads(pipe, pipe->writers);

    return readBytes;
}

static BOOL pipe_write_test_ready(File *file)
{
    Pipe* pipe = file->node->privateNodeData;

    begin_critical_section();
    int readerCount = List_GetCount(pipe->readers);
    end_critical_section();

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
    while (pipe_write_test_ready(file) == FALSE)
    {
        if (pipe->isBroken)
        {
            disableInterrupts();
            return -EPIPE;
        }

        if (gCurrentThread->pendingSignalCount > 0)
        {
            return -EINTR;
        }

        block_accessing_threads(pipe, pipe->writers);
    }

    if (gCurrentThread->pendingSignalCount > 0)
    {
        return -EINTR;
    }

    disableInterrupts();

    int32 bytesWritten = FifoBuffer_enqueue(pipe->buffer, buffer, size);

    wakeup_accessing_threads(pipe, pipe->readers);

    return bytesWritten;
}

BOOL pipe_create(const char* name, uint32 bufferSize)
{
    List_Foreach (n, g_pipe_list)
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
    pipe->fsNode->readTestReady = pipe_read_test_ready;
    pipe->fsNode->writeTestReady = pipe_write_test_ready;

    List_Append(g_pipe_list, pipe);

    return TRUE;
}

BOOL pipe_destroy(const char* name)
{
    List_Foreach (n, g_pipe_list)
    {
        Pipe* p = (Pipe*)n->data;
        if (strcmp(name, p->name) == 0)
        {
            List_RemoveFirstOccurrence(g_pipe_list, p);
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

BOOL pipe_exists(const char* name)
{
    List_Foreach (n, g_pipe_list)
    {
        Pipe* p = (Pipe*)n->data;
        if (strcmp(name, p->name) == 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}
