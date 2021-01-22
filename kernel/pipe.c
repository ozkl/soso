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

static BOOL pipes_open(File *file, uint32_t flags);
static FileSystemDirent *pipes_readdir(FileSystemNode *node, uint32_t index);
static FileSystemNode *pipes_finddir(FileSystemNode *node, char *name);

void pipe_initialize()
{
    g_pipe_list = list_create();

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

static BOOL pipes_open(File *file, uint32_t flags)
{
    return TRUE;
}

static FileSystemDirent *pipes_readdir(FileSystemNode *node, uint32_t index)
{
    int counter = 0;

    list_foreach (n, g_pipe_list)
    {
        Pipe* p = (Pipe*)n->data;

        if (counter == index)
        {
            strcpy(g_dirent.name, p->name);
            g_dirent.file_type = FT_PIPE;

            return &g_dirent;
        }
        ++counter;
    }

    return NULL;
}

static FileSystemNode *pipes_finddir(FileSystemNode *node, char *name)
{
    list_foreach (n, g_pipe_list)
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
    disable_interrupts();

    list_foreach (n, list)
    {
        Thread* reader = n->data;

        thread_change_state(reader, TS_WAITIO, pipe);
    }

    enable_interrupts();

    halt();
}

static void wakeup_accessing_threads(Pipe* pipe, List* list)
{
    begin_critical_section();

    list_foreach (n, list)
    {
        Thread* reader = n->data;

        if (reader->state == TS_WAITIO)
        {
            if (reader->state_privateData == pipe)
            {
                thread_resume(reader);
            }
        }
    }

    end_critical_section();
}

static BOOL pipe_open(File *file, uint32_t flags)
{
    if (CHECK_ACCESS(file->flags, O_RDONLY) || CHECK_ACCESS(file->flags, O_WRONLY))
    {
        begin_critical_section();

        g_current_thread->state = TS_CRITICAL;

        Pipe* pipe = file->node->private_node_data;

        pipe->isBroken = FALSE;

        if (CHECK_ACCESS(file->flags, O_RDONLY))
        {
            list_append(pipe->readers, file->thread);

            wakeup_accessing_threads(pipe, pipe->writers);
        }
        else if (CHECK_ACCESS(file->flags, O_WRONLY))
        {
            list_append(pipe->writers, file->thread);
        }

        g_current_thread->state = TS_RUN;

        end_critical_section();

        return TRUE;
    }

    return FALSE;
}

static void pipe_close(File *file)
{
    begin_critical_section();

    Pipe* pipe = file->node->private_node_data;

    if (CHECK_ACCESS(file->flags, O_RDONLY))
    {
        list_remove_first_occurrence(pipe->readers, file->thread);
    }
    else if (CHECK_ACCESS(file->flags, O_WRONLY))
    {
        list_remove_first_occurrence(pipe->writers, file->thread);
    }

    if (list_is_empty(pipe->readers))
    {
        //No readers left
        pipe->isBroken = TRUE;

        wakeup_accessing_threads(pipe, pipe->writers);
    }

    end_critical_section();
}

static BOOL pipe_read_test_ready(File *file)
{
    Pipe* pipe = file->node->private_node_data;

    if (fifobuffer_get_size(pipe->buffer) > 0)
    {
        return TRUE;
    }

    return FALSE;
}

static int32_t pipe_read(File *file, uint32_t size, uint8_t *buffer)
{
    if (0 == size || NULL == buffer || !CHECK_ACCESS(file->flags, O_RDONLY))
    {
        return -1;
    }

    Pipe* pipe = file->node->private_node_data;

    uint32_t used = 0;
    while (pipe_read_test_ready(file) == FALSE)
    {
        if (pipe->isBroken)
        {
            disable_interrupts();
            return -EPIPE;
        }

        if (g_current_thread->pending_signal_count > 0)
        {
            return -EINTR;
        }

        block_accessing_threads(pipe, pipe->readers);
    }

    if (g_current_thread->pending_signal_count > 0)
    {
        return -EINTR;
    }

    disable_interrupts();



    int32_t readBytes = fifobuffer_dequeue(pipe->buffer, buffer, size);

    wakeup_accessing_threads(pipe, pipe->writers);

    return readBytes;
}

static BOOL pipe_write_test_ready(File *file)
{
    Pipe* pipe = file->node->private_node_data;

    begin_critical_section();
    int readerCount = list_get_count(pipe->readers);
    end_critical_section();

    if (fifobuffer_get_free(pipe->buffer) > 0 && readerCount > 0)
    {
        return TRUE;
    }

    return FALSE;
}

static int32_t pipe_write(File *file, uint32_t size, uint8_t *buffer)
{
    if (0 == size || NULL == buffer || !CHECK_ACCESS(file->flags, O_WRONLY))
    {
        return -1;
    }

    Pipe* pipe = file->node->private_node_data;

    uint32_t free = 0;
    while (pipe_write_test_ready(file) == FALSE)
    {
        if (pipe->isBroken)
        {
            disable_interrupts();
            return -EPIPE;
        }

        if (g_current_thread->pending_signal_count > 0)
        {
            return -EINTR;
        }

        block_accessing_threads(pipe, pipe->writers);
    }

    if (g_current_thread->pending_signal_count > 0)
    {
        return -EINTR;
    }

    disable_interrupts();

    int32_t bytesWritten = fifobuffer_enqueue(pipe->buffer, buffer, size);

    wakeup_accessing_threads(pipe, pipe->readers);

    return bytesWritten;
}

BOOL pipe_create(const char* name, uint32_t bufferSize)
{
    list_foreach (n, g_pipe_list)
    {
        Pipe* p = (Pipe*)n->data;
        if (strcmp(name, p->name) == 0)
        {
            return FALSE;
        }
    }

    Pipe* pipe = (Pipe*)kmalloc(sizeof(Pipe));
    memset((uint8_t*)pipe, 0, sizeof(Pipe));

    pipe->isBroken = FALSE;

    strcpy(pipe->name, name);
    pipe->buffer = fifobuffer_create(bufferSize);

    pipe->readers = list_create();
    pipe->writers = list_create();

    pipe->fsNode = (FileSystemNode*)kmalloc(sizeof(FileSystemNode));
    memset((uint8_t*)pipe->fsNode, 0, sizeof(FileSystemNode));
    pipe->fsNode->private_node_data = pipe;
    pipe->fsNode->open = pipe_open;
    pipe->fsNode->close = pipe_close;
    pipe->fsNode->read = pipe_read;
    pipe->fsNode->write = pipe_write;
    pipe->fsNode->read_test_ready = pipe_read_test_ready;
    pipe->fsNode->write_test_ready = pipe_write_test_ready;

    list_append(g_pipe_list, pipe);

    return TRUE;
}

BOOL pipe_destroy(const char* name)
{
    list_foreach (n, g_pipe_list)
    {
        Pipe* p = (Pipe*)n->data;
        if (strcmp(name, p->name) == 0)
        {
            list_remove_first_occurrence(g_pipe_list, p);
            fifobuffer_destroy(p->buffer);
            list_destroy(p->readers);
            list_destroy(p->writers);
            kfree(p->fsNode);
            kfree(p);

            return TRUE;
        }
    }

    return FALSE;
}

BOOL pipe_exists(const char* name)
{
    list_foreach (n, g_pipe_list)
    {
        Pipe* p = (Pipe*)n->data;
        if (strcmp(name, p->name) == 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}
