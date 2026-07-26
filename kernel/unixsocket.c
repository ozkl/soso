/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

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
static BOOL unixsocket_fs_write_test_ready(File *file);
static int32_t unixsocket_fs_read(File *file, uint32_t len, uint8_t *buf);
static int32_t unixsocket_fs_write(File *file, uint32_t len, uint8_t *buf);

static int32_t unixsocket_get_name(const struct sockaddr* addr, socklen_t addrlen, char* name)
{
    if (addr == NULL)
    {
        return -EFAULT;
    }

    if (addrlen <= sizeof(sa_family_t) ||
        addrlen > sizeof(sa_family_t) + SOCKET_NAME_SIZE)
    {
        return -EINVAL;
    }

    if (!check_user_access((void*)addr) ||
        !check_user_access((uint8_t*)addr + addrlen - 1))
    {
        return -EFAULT;
    }

    if (addr->sa_family != AF_UNIX)
    {
        return -EAFNOSUPPORT;
    }

    const char* source = (const char*)addr + sizeof(sa_family_t);
    uint32_t available = addrlen - sizeof(sa_family_t);
    uint32_t length = 0;

    if (source[0] == '\0')
    {
        return -EOPNOTSUPP;
    }

    while (length < available && source[length] != '\0')
    {
        ++length;
    }

    if (length == 0)
    {
        return -EINVAL;
    }

    if (length >= SOCKET_NAME_SIZE)
    {
        return -ENAMETOOLONG;
    }

    memcpy((uint8_t*)name, (const uint8_t*)source, length);
    name[length] = '\0';

    return 0;
}

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
    socket->node->write_test_ready = unixsocket_fs_write_test_ready;
    socket->node->read = unixsocket_fs_read;
    socket->node->write = unixsocket_fs_write;
}

static int unixsocket_bind(Socket* socket, int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    //printkf("unixsocket_bind\n");

    UnixSocket* unix_socket = (UnixSocket*)socket->custom_socket;
    char name[SOCKET_NAME_SIZE];
    int32_t result = unixsocket_get_name(addr, addrlen, name);
    if (result < 0)
    {
        return result;
    }

    if (strlen(unix_socket->name) > 0)
    {
        return -EINVAL; //The socket is already bound to an address.
    }

    list_foreach (n, g_socket_list)
    {
        Socket* s = (Socket*)n->data;
        if (s->domain != AF_UNIX || s->custom_socket == NULL)
        {
            continue;
        }

        UnixSocket* us = (UnixSocket*)s->custom_socket;

        if (strcmp(name, us->name) == 0)
        {
            return -EADDRINUSE;
        }
    }

    strcpy(unix_socket->name, name);

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

    if ((addr == NULL) != (addrlen == NULL))
    {
        return -EFAULT;
    }

    if (addr != NULL &&
        (!check_user_access(addr) ||
         !check_user_access(addrlen) ||
         !check_user_access((uint8_t*)addrlen + sizeof(socklen_t) - 1)))
    {
        return -EFAULT;
    }

    if (addr != NULL &&
        *addrlen >= sizeof(sa_family_t) &&
        !check_user_access((uint8_t*)addr + sizeof(sa_family_t) - 1))
    {
        return -EFAULT;
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
                File* file = process_get_file(g_current_thread->owner, new_socket_fd);

                if (file)
                {
                    Socket* new_socket = (Socket*)file->node->private_node_data;

                    new_socket->connection = other_end;
                    other_end->connection = new_socket;
                    other_end->connecting_to = NULL;
                    waitqueue_wake_all(&other_end->connect_waiters, other_end);

                    if (addr != NULL)
                    {
                        socklen_t available = *addrlen;
                        *addrlen = sizeof(sa_family_t);
                        if (available >= sizeof(sa_family_t))
                        {
                            addr->sa_family = AF_UNIX;
                        }
                    }

                    return new_socket_fd;
                }
            }

            queue_enqueue(socket->accept_queue, other_end);
            return new_socket_fd < 0 ? new_socket_fd : -EMFILE;
        }

        File* file = process_get_file(g_current_thread->owner, sockfd);
        if (file && (file->status_flags & O_NONBLOCK))
        {
            return -EAGAIN;
        }

        if (g_current_thread->pending_signal_count > 0)
        {
            return -EINTR;
        }

        waitqueue_wait(&socket->accept_waiters, socket);
    }

    return -1;
}

static int unixsocket_connect(Socket* socket, int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    char name[SOCKET_NAME_SIZE];
    int32_t result = unixsocket_get_name(addr, addrlen, name);
    if (result < 0)
    {
        return result;
    }

    if (socket->connection != NULL)
    {
        return -EISCONN; //The socket is already connected.
    }

    if (socket->connecting_to != NULL)
    {
        return -EALREADY;
    }

    Socket* accepting_socket = NULL;
    list_foreach (n, g_socket_list)
    {
        Socket* s = (Socket*)n->data;
        if (s->domain != AF_UNIX || s->custom_socket == NULL)
        {
            continue;
        }

        UnixSocket* us = (UnixSocket*)s->custom_socket;

        if (strcmp(name, us->name) == 0)
        {
            accepting_socket = s;
            break;
        }
    }

    if (accepting_socket && BITMAP_CHECK(accepting_socket->opts, SO_ACCEPTCONN))
    {
        socket->connecting_to = accepting_socket;
        queue_enqueue(accepting_socket->accept_queue, socket);

        waitqueue_wake_all(&accepting_socket->accept_waiters, accepting_socket);

        File* file = process_get_file(g_current_thread->owner, sockfd);
        if (file && (file->status_flags & O_NONBLOCK))
        {
            return -EINPROGRESS;
        }

        while (TRUE)
        {
            disable_interrupts();

            if (socket->connection != NULL)
            {
                return 0;
            }

            if (socket->disconnected)
            {
                return -ECONNREFUSED;
            }

            if (g_current_thread->pending_signal_count > 0)
            {
                return -EINTR;
            }

            waitqueue_wait(&socket->connect_waiters, socket);
        }
    }

    return accepting_socket == NULL ? -ENOENT : -ECONNREFUSED;
}

static ssize_t unixsocket_send(Socket* socket, int sockfd, const void *buf, size_t len, int flags)
{
    if (len == 0)
    {
        return 0;
    }


    while (TRUE)
    {
        disable_interrupts();

        if (!socket->connection)
        {
            return -EPIPE;
        }

        uint32_t free = fifobuffer_get_free(socket->connection->buffer_in);

        if (free > 0)
        {
            uint32_t smaller = MIN(free, len);

            uint32_t written = fifobuffer_enqueue(socket->connection->buffer_in, (uint8_t*)buf, smaller);
            socket->connection->bytes_enqueued += written;

            waitqueue_wake_all(&socket->connection->read_waiters, socket->connection);

            return written;
        }
        else
        {
            File* file = process_get_file(g_current_thread->owner, sockfd);
            if ((flags & MSG_DONTWAIT) ||
                (file && (file->status_flags & O_NONBLOCK)))
            {
                return -EAGAIN;
            }

            if (g_current_thread->pending_signal_count > 0)
            {
                return -EINTR;
            }

            waitqueue_wait(&socket->write_waiters, socket);
        }
    }

    return -1;
}

static ssize_t unixsocket_recv(Socket* socket, int sockfd, void *buf, size_t len, int flags)
{
    if (len == 0)
    {
        return 0;
    }

    while (TRUE)
    {
        disable_interrupts();

        uint32_t size = fifobuffer_get_size(socket->buffer_in);

        if (size > 0)
        {
            uint32_t smaller = MIN(size, len);

            uint32_t read = fifobuffer_dequeue(socket->buffer_in, (uint8_t*)buf, smaller);
            socket->bytes_dequeued += read;

            if (socket->connection)
            {
                waitqueue_wake_all(&socket->connection->write_waiters, socket->connection);
            }

            return read;
        }

        if (socket->disconnected)
        {
            return 0;
        }

        File* file = process_get_file(g_current_thread->owner, sockfd);
        if ((flags & MSG_DONTWAIT) ||
            (file && (file->status_flags & O_NONBLOCK)))
        {
            return -EAGAIN;
        }

        if (g_current_thread->pending_signal_count > 0)
        {
            return -EINTR;
        }

        waitqueue_wait(&socket->read_waiters, socket);
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

static BOOL unixsocket_fs_write_test_ready(File *file)
{
    Socket* socket = (Socket*)file->node->private_node_data;

    if (socket->connection == NULL)
    {
        return socket->connecting_to == NULL || socket->disconnected;
    }

    return fifobuffer_get_free(socket->connection->buffer_in) > 0;
}

static int32_t unixsocket_fs_read(File *file, uint32_t len, uint8_t *buf)
{
    Socket* socket = (Socket*)file->node->private_node_data;

    int32_t flags = (file->status_flags & O_NONBLOCK) ? MSG_DONTWAIT : 0;
    int32_t result = unixsocket_recv(socket, -1, buf, len, flags);
    if (result > 0)
    {
        socket_discard_consumed_control(socket);
    }
    return result;
}

static int32_t unixsocket_fs_write(File *file, uint32_t len, uint8_t *buf)
{
    Socket* socket = (Socket*)file->node->private_node_data;

    int32_t flags = (file->status_flags & O_NONBLOCK) ? MSG_DONTWAIT : 0;
    return unixsocket_send(socket, -1, (const uint8_t *)buf, len, flags);
}
