/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#include "list.h"
#include "pipe.h"
#include "fs.h"
#include "alloc.h"
#include "fifobuffer.h"
#include "errno.h"
#include "waitqueue.h"

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
    WaitQueue reader_waiters;
    WaitQueue writer_waiters;
} Pipe;

static BOOL pipes_open(File *file, uint32_t flags);
static FileSystemDirent *pipes_readdir(FileSystemNode *node, uint32_t index);
static FileSystemNode *pipes_finddir(FileSystemNode *node, char *name);

static BOOL pipe_can_read(File* file)
{
    return CHECK_ACCESS(file->status_flags, O_RDONLY) ||
           CHECK_ACCESS(file->status_flags, O_RDWR);
}

static BOOL pipe_can_write(File* file)
{
    return CHECK_ACCESS(file->status_flags, O_WRONLY) ||
           CHECK_ACCESS(file->status_flags, O_RDWR);
}

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

static BOOL pipe_open(File *file, uint32_t flags)
{
    if (pipe_can_read(file) || pipe_can_write(file))
    {
        begin_critical_section();

        Pipe* pipe = file->node->private_node_data;

        if (pipe_can_read(file))
        {
            list_append(pipe->readers, file);

            waitqueue_wake_all(&pipe->writer_waiters, pipe);
        }

        if (pipe_can_write(file))
        {
            list_append(pipe->writers, file);

            waitqueue_wake_all(&pipe->reader_waiters, pipe);
        }

        end_critical_section();

        return TRUE;
    }

    return FALSE;
}

static void pipe_close(File *file)
{
    begin_critical_section();

    Pipe* pipe = file->node->private_node_data;

    if (pipe_can_read(file))
    {
        list_remove_first_occurrence(pipe->readers, file);

        if (list_is_empty(pipe->readers))
        {
            waitqueue_wake_all(&pipe->writer_waiters, pipe);
        }
    }
    if (pipe_can_write(file))
    {
        list_remove_first_occurrence(pipe->writers, file);

        if (list_is_empty(pipe->writers))
        {
            waitqueue_wake_all(&pipe->reader_waiters, pipe);
        }
    }

    end_critical_section();
}

static BOOL pipe_read_test_ready(File *file)
{
    Pipe* pipe = file->node->private_node_data;

    begin_critical_section();
    BOOL ready = fifobuffer_get_size(pipe->buffer) > 0 ||
                 list_is_empty(pipe->writers);
    end_critical_section();

    return ready;
}

static int32_t pipe_read(File *file, uint32_t size, uint8_t *buffer)
{
    if (0 == size || NULL == buffer || !pipe_can_read(file))
    {
        return -1;
    }

    Pipe* pipe = file->node->private_node_data;

    while (TRUE)
    {
        disable_interrupts();

        if (fifobuffer_get_size(pipe->buffer) > 0)
        {
            break;
        }

        if (list_is_empty(pipe->writers))
        {
            return 0;
        }

        if (file->status_flags & O_NONBLOCK)
        {
            return -EAGAIN;
        }

        if (g_current_thread->pending_signal_count > 0)
        {
            return -EINTR;
        }

        waitqueue_wait(&pipe->reader_waiters, pipe);
    }

    int32_t readBytes = fifobuffer_dequeue(pipe->buffer, buffer, size);

    waitqueue_wake_all(&pipe->writer_waiters, pipe);

    return readBytes;
}

static BOOL pipe_write_test_ready(File *file)
{
    Pipe* pipe = file->node->private_node_data;

    begin_critical_section();
    BOOL ready = list_is_empty(pipe->readers) ||
                 fifobuffer_get_free(pipe->buffer) > 0;
    end_critical_section();

    return ready;
}

static int32_t pipe_write(File *file, uint32_t size, uint8_t *buffer)
{
    if (0 == size || NULL == buffer || !pipe_can_write(file))
    {
        return -1;
    }

    Pipe* pipe = file->node->private_node_data;
    BOOL atomic_write = size <= fifobuffer_get_capacity(pipe->buffer);

    while (TRUE)
    {
        disable_interrupts();

        if (list_is_empty(pipe->readers))
        {
            return -EPIPE;
        }

        uint32_t free = fifobuffer_get_free(pipe->buffer);
        if ((atomic_write && free >= size) ||
            (!atomic_write && free > 0))
        {
            break;
        }

        if (file->status_flags & O_NONBLOCK)
        {
            return -EAGAIN;
        }

        if (g_current_thread->pending_signal_count > 0)
        {
            return -EINTR;
        }

        waitqueue_wait(&pipe->writer_waiters, pipe);
    }

    int32_t bytesWritten = fifobuffer_enqueue(pipe->buffer, buffer, size);

    waitqueue_wake_all(&pipe->reader_waiters, pipe);

    return bytesWritten;
}

BOOL pipe_create(const char* name, uint32_t bufferSize)
{
    if (name == NULL || strlen(name) >= sizeof(((Pipe*)0)->name) || bufferSize == 0)
    {
        return FALSE;
    }

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

    strncpy_null(pipe->name, name, sizeof(pipe->name));
    pipe->buffer = fifobuffer_create(bufferSize);

    pipe->readers = list_create();
    pipe->writers = list_create();
    waitqueue_initialize(&pipe->reader_waiters);
    waitqueue_initialize(&pipe->writer_waiters);

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
            begin_critical_section();

            if (!list_is_empty(p->readers) ||
                !list_is_empty(p->writers) ||
                !waitqueue_is_empty(&p->reader_waiters) ||
                !waitqueue_is_empty(&p->writer_waiters))
            {
                end_critical_section();
                return FALSE;
            }

            list_remove_first_occurrence(g_pipe_list, p);
            fifobuffer_destroy(p->buffer);
            list_destroy(p->readers);
            list_destroy(p->writers);
            waitqueue_destroy(&p->reader_waiters);
            waitqueue_destroy(&p->writer_waiters);
            kfree(p->fsNode);
            kfree(p);

            end_critical_section();
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
