#include "unixsocket.h"
#include "fifobuffer.h"
#include "list.h"
#include "fs.h"
#include "alloc.h"
#include "common.h"
#include "socket.h"
#include "errno.h"
typedef struct
{
    Socket* owner;
    char name[SOCKET_NAME_SIZE];
} UnixSocket;

static void unixsocket_closing(Socket* socket);
static int unixsocket_bind(Socket* socket, int sockfd, const struct sockaddr *addr, socklen_t addrlen);
static int unixsocket_listen(Socket* socket, int sockfd, int backlog);
static int unixsocket_accept(Socket* socket, int sockfd, struct sockaddr *addr, socklen_t *addrlen);
static int unixsocket_connect(Socket* socket, int sockfd, const struct sockaddr *addr, socklen_t addrlen);
static ssize_t unixsocket_send(Socket* socket, int sockfd, const void *buf, size_t len, int flags);
static ssize_t unixsocket_recv(Socket* socket, int sockfd, void *buf, size_t len, int flags);

static BOOL unixsocket_fs_read_test_ready(File *file);
static int32_t unixsocket_fs_read(File *file, uint32_t len, uint8_t *buf);
static int32_t unixsocket_fs_write(File *file, uint32_t len, uint8_t *buf);

void unixsocket_setup(Socket* socket)
{
    //printkf("unixsocket_setup\n");

    UnixSocket* unix_socket = (UnixSocket*)kmalloc(sizeof(UnixSocket));
    memset((uint8_t*)unix_socket, 0, sizeof(UnixSocket));

    socket->custom_socket = unix_socket;
    unix_socket->owner = socket;

    socket->socket_closing = unixsocket_closing;
    socket->socket_bind = unixsocket_bind;
    socket->socket_listen = unixsocket_listen;
    socket->socket_accept = unixsocket_accept;
    socket->socket_connect = unixsocket_connect;
    socket->socket_send = unixsocket_send;
    socket->socket_recv = unixsocket_recv;

    socket->node->read_test_ready = unixsocket_fs_read_test_ready;
    socket->node->read = unixsocket_fs_read;
    socket->node->write = unixsocket_fs_write;
}

static int unixsocket_bind(Socket* socket, int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    //printkf("unixsocket_bind\n");

    UnixSocket* unix_socket = (UnixSocket*)socket->custom_socket;

    if (strlen(unix_socket->name) > 0)
    {
        return -EINVAL; //The socket is already bound to an address.
    }

    if (strlen(addr->sa_data) == 0)
    {
        return -EINVAL;
    }

    list_foreach (n, g_socket_list)
    {
        Socket* s = (Socket*)n->data;
        UnixSocket* us = (UnixSocket*)s->custom_socket;

        if (strcmp(addr->sa_data, us->name) == 0)
        {
            return -EADDRINUSE;
        }
    }

    sprintf(unix_socket->name, SOCKET_NAME_SIZE, addr->sa_data);

    return 0; //success
}

static int unixsocket_listen(Socket* socket, int sockfd, int backlog)
{
    UnixSocket* unix_socket = (UnixSocket*)socket->custom_socket;
    
    BITMAP_SET(socket->opts, SO_ACCEPTCONN);

    return 0;
}

static int unixsocket_accept(Socket* socket, int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    UnixSocket* unix_socket = (UnixSocket*)socket->custom_socket;
    
    if (!BITMAP_CHECK(socket->opts, SO_ACCEPTCONN))
    {
        //Socket is not listening for connections

        return -EINVAL;
    }

    while (TRUE)
    {
        disable_interrupts();

        Socket* other_end = NULL;

        if (queue_is_empty(socket->accept_queue) == FALSE)
        {
            other_end = (Socket*)queue_dequeue(socket->accept_queue);
        }

        if (other_end)
        {
            int new_socket_fd = syscall_socket(socket->domain, 1, 0);

            if (new_socket_fd >= 0 && new_socket_fd < SOSO_MAX_OPENED_FILES)
            {
                File* file = g_current_thread->owner->fd[new_socket_fd];

                if (file)
                {
                    Socket* new_socket = (Socket*)file->node->private_node_data;

                    new_socket->connection = other_end;
                    other_end->connection = new_socket;

                    return new_socket_fd;
                }
            }
        }

        thread_change_state(g_current_thread, TS_WAITIO, unixsocket_accept);
        enable_interrupts();
        halt();
    }

    return -1;
}

static int unixsocket_connect(Socket* socket, int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    if (strlen(addr->sa_data) == 0)
    {
        return -EINVAL;
    }

    if (socket->connection != NULL)
    {
        return -EISCONN; //The socket is already connected.
    }

    Socket* accepting_socket = NULL;
    list_foreach (n, g_socket_list)
    {
        Socket* s = (Socket*)n->data;
        UnixSocket* us = (UnixSocket*)s->custom_socket;

        if (strcmp(addr->sa_data, us->name) == 0)
        {
            accepting_socket = s;
            break;
        }
    }

    if (accepting_socket && BITMAP_CHECK(accepting_socket->opts, SO_ACCEPTCONN))
    {
        queue_enqueue(accepting_socket->accept_queue, socket);

        if (accepting_socket->last_thread->state == TS_WAITIO && accepting_socket->last_thread->state_privateData == unixsocket_accept)
        {
            thread_resume(accepting_socket->last_thread);
        }

        while (socket->connection == NULL)
        {
            enable_interrupts();
            halt();
        }
        return 0;
    }

    return -1;
}

static ssize_t unixsocket_send(Socket* socket, int sockfd, const void *buf, size_t len, int flags)
{
    if (len == 0)
    {
        return -1;
    }


    while (TRUE)
    {
        disable_interrupts();

        if (!socket->connection)
        {
            return -1;
        }

        uint32_t free = fifobuffer_get_free(socket->connection->buffer_in);

        if (free > 0)
        {
            uint32_t smaller = MIN(free, len);

            uint32_t written = fifobuffer_enqueue(socket->connection->buffer_in, (uint8_t*)buf, smaller);

            if (socket->connection->last_thread->state == TS_WAITIO && socket->connection->last_thread->state_privateData == unixsocket_recv)
            {
                thread_resume(socket->connection->last_thread);
            }

            return written;
        }
        else
        {
            thread_change_state(g_current_thread, TS_WAITIO, unixsocket_send);
            enable_interrupts();
            halt();
        }
    }

    return -1;
}

static ssize_t unixsocket_recv(Socket* socket, int sockfd, void *buf, size_t len, int flags)
{
    if (len == 0)
    {
        return -1;
    }

    while (TRUE)
    {
        disable_interrupts();

        if (socket->disconnected)
        {
            return 0;
        }

        uint32_t size = fifobuffer_get_size(socket->buffer_in);

        if (size > 0)
        {
            uint32_t smaller = MIN(size, len);

            uint32_t read = fifobuffer_dequeue(socket->buffer_in, (uint8_t*)buf, smaller);

            if (socket->connection->last_thread->state == TS_WAITIO && socket->connection->last_thread->state_privateData == unixsocket_send)
            {
                thread_resume(socket->connection->last_thread);
            }

            return read;
        }

        thread_change_state(g_current_thread, TS_WAITIO, unixsocket_recv);
        enable_interrupts();
        halt();
    }

    return -1;
}

static void unixsocket_closing(Socket* socket)
{
    //printkf("unixsocket_closing\n");

    UnixSocket* unix_socket = (UnixSocket*)socket->custom_socket;

    kfree(unix_socket);

    socket->custom_socket = NULL;
}

static BOOL unixsocket_fs_read_test_ready(File *file)
{
    Socket* socket = (Socket*)file->node->private_node_data;

    UnixSocket* unix_socket = (UnixSocket*)socket->custom_socket;

    if (queue_is_empty(socket->accept_queue) == FALSE)
    {
        return TRUE;
    }

    if (fifobuffer_get_size(socket->buffer_in) > 0)
    {
        return TRUE;
    }

    if (socket->disconnected)
    {
        return TRUE;
    }

    return FALSE;
}

static int32_t unixsocket_fs_read(File *file, uint32_t len, uint8_t *buf)
{
    Socket* socket = (Socket*)file->node->private_node_data;

    return unixsocket_recv(socket, file->fd, buf, len, 0);
}

static int32_t unixsocket_fs_write(File *file, uint32_t len, uint8_t *buf)
{
    Socket* socket = (Socket*)file->node->private_node_data;

    return unixsocket_send(socket, file->fd, (const uint8_t *)buf, len, 0);
}