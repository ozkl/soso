/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#include "socket.h"
#include "unixsocket.h"
#include "errno.h"
#include "common.h"
#include "process.h"
#include "list.h"
#include "fs.h"
#include "alloc.h"
#include "timer.h"

#define DOMAIN_SIZE 3
#define SOCKET_MAX_RIGHTS SOSO_MAX_OPENED_FILES
#define CMSG_ALIGN(length) (((length) + sizeof(uint32_t) - 1) & ~(sizeof(uint32_t) - 1))
#define FIONBIO 0x5421

typedef struct SocketControlMessage
{
    uint64_t byte_offset;
    uint32_t file_count;
    File* files[SOCKET_MAX_RIGHTS];
} SocketControlMessage;

List* g_socket_list = NULL;


static SocketFunction g_domain_setup_functions[DOMAIN_SIZE];

uint64_t socket_timeout_deadline(uint32_t timeout_ms)
{
    if (timeout_ms == 0)
    {
        return 0;
    }

    uint64_t now = get_uptime_milliseconds64();
    if (timeout_ms > 0xFFFFFFFFFFFFFFFFULL - now)
    {
        return 0xFFFFFFFFFFFFFFFFULL;
    }

    return now + timeout_ms;
}

static void socket_control_message_destroy(SocketControlMessage* message)
{
    for (uint32_t i = 0; i < message->file_count; ++i)
    {
        if (message->files[i] != NULL)
        {
            fs_release(message->files[i]);
        }
    }

    kfree(message);
}

void socket_discard_consumed_control(Socket* socket)
{
    while (!queue_is_empty(socket->control_queue))
    {
        ListNode* node = list_get_first_node(socket->control_queue->list);
        SocketControlMessage* message = (SocketControlMessage*)node->data;
        if (message->byte_offset >= socket->bytes_dequeued)
        {
            break;
        }

        queue_dequeue(socket->control_queue);
        socket_control_message_destroy(message);
    }
}

void net_initialize()
{
    g_socket_list = list_create();

    for (size_t i = 0; i < DOMAIN_SIZE; i++)
    {
        g_domain_setup_functions[i] = NULL;
    }
    g_domain_setup_functions[AF_UNIX] = unixsocket_setup;
}

static Socket* get_socket(int sockfd, int* error)
{
    Process* process = g_current_thread->owner;
    if (process)
    {
        if (sockfd >= 0 && sockfd < SOSO_MAX_OPENED_FILES)
        {
            File* file = process_get_file(process, sockfd);

            if (file)
            {
                if (file->node->node_type != FT_SOCKET)
                {
                    *error = -ENOTSOCK;
                    return NULL;
                }

                Socket* socket = (Socket*)file->node->private_node_data;

                return socket;
            }
            else
            {
                *error = -EBADF;
                return NULL;
            }
            
        }
        else
        {
            *error = -EBADF;
            return NULL;
        }
        
    }

    *error = -1;
    return NULL;
}

static Socket* socket_create()
{
    Socket* socket = (Socket*)kmalloc(sizeof(Socket));
    memset((uint8_t*)socket, 0, sizeof(Socket));

    socket->buffer_out = fifobuffer_create(SOCKET_BUFFER_SIZE);
    socket->buffer_in = fifobuffer_create(SOCKET_BUFFER_SIZE);

    socket->accept_queue = queue_create();
    socket->control_queue = queue_create();
    waitqueue_initialize(&socket->accept_waiters);
    waitqueue_initialize(&socket->connect_waiters);
    waitqueue_initialize(&socket->read_waiters);
    waitqueue_initialize(&socket->write_waiters);

    list_append(g_socket_list, socket);

    return socket;
}

static void socket_destroy(Socket* socket)
{
    list_remove_first_occurrence(g_socket_list, socket);

    fifobuffer_destroy(socket->buffer_out);
    fifobuffer_destroy(socket->buffer_in);

    while (!queue_is_empty(socket->accept_queue))
    {
        Socket* pending = (Socket*)queue_dequeue(socket->accept_queue);
        pending->connecting_to = NULL;
        pending->disconnected = TRUE;
        waitqueue_wake_all(&pending->connect_waiters, pending);
    }
    queue_destroy(socket->accept_queue);

    while (!queue_is_empty(socket->control_queue))
    {
        SocketControlMessage* message = queue_dequeue(socket->control_queue);
        socket_control_message_destroy(message);
    }
    queue_destroy(socket->control_queue);

    waitqueue_destroy(&socket->accept_waiters);
    waitqueue_destroy(&socket->connect_waiters);
    waitqueue_destroy(&socket->read_waiters);
    waitqueue_destroy(&socket->write_waiters);

    kfree(socket);
}

static BOOL socket_fs_open(File* file, uint32_t flags)
{
    Socket* socket = (Socket*)file->node->private_node_data;

    SocketFunction setup = g_domain_setup_functions[socket->domain];

    if (setup)
    {
        setup(socket);
    }

    return TRUE;
}

static int32_t socket_fs_ioctl(File* file, int32_t request, void* argp)
{
    if (request != FIONBIO)
    {
        return -ENOTTY;
    }

    if (argp == NULL ||
        !check_user_access(argp) ||
        !check_user_access((uint8_t*)argp + sizeof(int32_t) - 1))
    {
        return -EFAULT;
    }

    if (*(int32_t*)argp)
    {
        file->status_flags |= O_NONBLOCK;
    }
    else
    {
        file->status_flags &= ~O_NONBLOCK;
    }

    return 0;
}

static void socket_fs_close(File* file)
{
    Socket* socket = (Socket*)file->node->private_node_data;

    if (socket->connecting_to)
    {
        list_remove_first_occurrence(socket->connecting_to->accept_queue->list, socket);
        socket->connecting_to = NULL;
    }

    if (socket->socket_closing)
    {
        socket->socket_closing(socket);
    }

    if (socket->connection)
    {
        Socket* connection = socket->connection;
        connection->connection = NULL;
        connection->disconnected = TRUE;

        waitqueue_wake_all(&connection->read_waiters, connection);
        waitqueue_wake_all(&connection->write_waiters, connection);
    }

    kfree(socket->node);
    socket->node = NULL;

    socket_destroy(socket);
}

int syscall_socket(int domain, int type, int protocol)
{
    //printkf("socket %d %d %d\n", domain, type, protocol);

    int32_t socket_type = type & 0xF;
    if (domain >= 0 && domain < DOMAIN_SIZE &&
        socket_type > 0 &&
        (type & ~(0xF | SOCK_CLOEXEC | SOCK_NONBLOCK)) == 0)
    {
        Socket* socket = socket_create();

        socket->domain = domain;

        FileSystemNode* node = (FileSystemNode*)kmalloc(sizeof(FileSystemNode));
        memset((uint8_t*)node, 0, sizeof(FileSystemNode));

        socket->node = node;
        node->private_node_data = socket;

        node->node_type = FT_SOCKET;

        node->open = socket_fs_open;
        node->close = socket_fs_close;
        node->ioctl = socket_fs_ioctl;

        uint32_t open_flags = O_RDWR;
        if (type & SOCK_CLOEXEC)
        {
            open_flags |= O_CLOEXEC;
        }
        if (type & SOCK_NONBLOCK)
        {
            open_flags |= O_NONBLOCK;
        }

        return fs_open_fd_for_process(g_current_thread, node, open_flags, NULL);
    }

    return -EPERM;
}

int syscall_socketpair(int domain, int type, int protocol, int sv[2])
{
    if (!check_user_access(sv) || !check_user_access(&sv[1]))
    {
        return -EFAULT;
    }

    if (domain != AF_UNIX || (type & 0xF) != SOCK_STREAM)
    {
        return -EOPNOTSUPP;
    }

    int32_t first_fd = syscall_socket(domain, type, protocol);
    if (first_fd < 0)
    {
        return first_fd;
    }

    int32_t second_fd = syscall_socket(domain, type, protocol);
    if (second_fd < 0)
    {
        process_close_file(g_current_thread->owner, first_fd);
        return second_fd;
    }

    int32_t error = 0;
    Socket* first = get_socket(first_fd, &error);
    Socket* second = get_socket(second_fd, &error);

    first->connection = second;
    second->connection = first;

    sv[0] = first_fd;
    sv[1] = second_fd;

    return 0;
}

int syscall_shutdown(int sockfd, int how)
{
    return -EPERM;
}

int syscall_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    int error = -1;
    Socket* socket = get_socket(sockfd, &error);

    if (socket && socket->socket_bind)
    {
        return socket->socket_bind(socket, sockfd, addr, addrlen);
    }

    return error;
}

int syscall_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    int error = -1;
    Socket* socket = get_socket(sockfd, &error);

    if (socket && socket->socket_connect)
    {
        return socket->socket_connect(socket, sockfd, addr, addrlen);
    }

    return error;
}

int syscall_listen(int sockfd, int backlog)
{
    int error = -1;
    Socket* socket = get_socket(sockfd, &error);

    if (socket && socket->socket_listen)
    {
        return socket->socket_listen(socket, sockfd, backlog);
    }

    return error;
}

int syscall_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    int error = -1;
    Socket* socket = get_socket(sockfd, &error);

    if (socket && socket->socket_accept)
    {
        return socket->socket_accept(socket, sockfd, addr, addrlen);
    }

    return error;
}

int syscall_accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags)
{
    if (flags & ~(SOCK_CLOEXEC | SOCK_NONBLOCK))
    {
        return -EINVAL;
    }

    int32_t fd = syscall_accept(sockfd, addr, addrlen);
    if (fd < 0)
    {
        return fd;
    }

    FileDescriptor* descriptor = process_get_file_descriptor(g_current_thread->owner, fd);
    if (flags & SOCK_CLOEXEC)
    {
        descriptor->flags |= FD_CLOEXEC;
    }
    if (flags & SOCK_NONBLOCK)
    {
        descriptor->file->status_flags |= O_NONBLOCK;
    }

    return fd;
}


int syscall_getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    return -EPERM;
}

int syscall_getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    return -EPERM;
}


ssize_t syscall_send(int sockfd, const void *buf, size_t len, int flags)
{
    int error = -1;
    Socket* socket = get_socket(sockfd, &error);

    if (socket && socket->socket_send)
    {
        uint64_t target_time = socket_timeout_deadline(socket->send_timeout_ms);
        return socket->socket_send(socket, sockfd, buf, len, flags, target_time);
    }

    return error;
}

ssize_t syscall_recv(int sockfd, void *buf, size_t len, int flags)
{
    int error = -1;
    Socket* socket = get_socket(sockfd, &error);

    if (socket && socket->socket_recv)
    {
        uint64_t target_time = socket_timeout_deadline(socket->receive_timeout_ms);
        ssize_t result = socket->socket_recv(socket, sockfd, buf, len, flags, target_time);
        if (result > 0)
        {
            socket_discard_consumed_control(socket);
        }
        return result;
    }

    return error;
}

ssize_t syscall_sendto(int sockfd, const void *buf, size_t len, int flags,
                      const struct sockaddr *dest_addr, socklen_t addrlen)
{
    if (len > 0 &&
        (buf == NULL ||
         !check_user_access((void*)buf) ||
         !check_user_access((uint8_t*)buf + len - 1)))
    {
        return -EFAULT;
    }

    if (dest_addr != NULL || addrlen != 0)
    {
        return -EOPNOTSUPP;
    }

    return syscall_send(sockfd, buf, len, flags);
}

ssize_t syscall_recvfrom(int sockfd, void *buf, size_t len, int flags,
                        struct sockaddr *src_addr, socklen_t *addrlen)
{
    if (len > 0 &&
        (buf == NULL ||
         !check_user_access(buf) ||
         !check_user_access((uint8_t*)buf + len - 1)))
    {
        return -EFAULT;
    }

    if (src_addr != NULL || addrlen != NULL)
    {
        return -EOPNOTSUPP;
    }

    return syscall_recv(sockfd, buf, len, flags);
}

static SocketControlMessage* socket_get_send_control(const struct msghdr* msg, int32_t* error)
{
    *error = 0;

    if (msg->msg_controllen == 0)
    {
        return NULL;
    }

    if (msg->msg_control == NULL ||
        msg->msg_controllen > 4096 ||
        !check_user_access(msg->msg_control) ||
        !check_user_access((uint8_t*)msg->msg_control + msg->msg_controllen - 1))
    {
        *error = -EFAULT;
        return NULL;
    }

    SocketControlMessage* message = kmalloc(sizeof(SocketControlMessage));
    memset((uint8_t*)message, 0, sizeof(SocketControlMessage));

    uint32_t offset = 0;
    while (offset + sizeof(struct cmsghdr) <= msg->msg_controllen)
    {
        struct cmsghdr* header = (struct cmsghdr*)((uint8_t*)msg->msg_control + offset);
        if (header->cmsg_len < sizeof(struct cmsghdr) ||
            header->cmsg_len > msg->msg_controllen - offset)
        {
            *error = -EINVAL;
            socket_control_message_destroy(message);
            return NULL;
        }

        if (header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS)
        {
            *error = -EINVAL;
            socket_control_message_destroy(message);
            return NULL;
        }

        uint32_t data_length = header->cmsg_len - sizeof(struct cmsghdr);
        if (data_length == 0 || data_length % sizeof(int32_t) != 0)
        {
            *error = -EINVAL;
            socket_control_message_destroy(message);
            return NULL;
        }

        uint32_t file_count = data_length / sizeof(int32_t);
        if (message->file_count + file_count > SOCKET_MAX_RIGHTS)
        {
            *error = -EMSGSIZE;
            socket_control_message_destroy(message);
            return NULL;
        }

        int32_t* fds = (int32_t*)((uint8_t*)header + sizeof(struct cmsghdr));
        for (uint32_t i = 0; i < file_count; ++i)
        {
            File* file = process_get_file(g_current_thread->owner, fds[i]);
            if (file == NULL)
            {
                *error = -EBADF;
                socket_control_message_destroy(message);
                return NULL;
            }

            fs_acquire(file);
            message->files[message->file_count++] = file;
        }

        offset += CMSG_ALIGN(header->cmsg_len);
    }

    if (message->file_count == 0)
    {
        kfree(message);
        return NULL;
    }

    return message;
}

static void socket_receive_control(Socket* socket, struct msghdr* msg, int32_t flags)
{
    uint32_t capacity = msg->msg_controllen;
    uint32_t used = 0;
    msg->msg_controllen = 0;

    while (!queue_is_empty(socket->control_queue))
    {
        ListNode* node = list_get_first_node(socket->control_queue->list);
        SocketControlMessage* message = (SocketControlMessage*)node->data;
        if (message->byte_offset >= socket->bytes_dequeued)
        {
            break;
        }

        queue_dequeue(socket->control_queue);

        uint32_t available_fds = 0;
        if (msg->msg_control != NULL && capacity - used >= sizeof(struct cmsghdr))
        {
            available_fds = (capacity - used - sizeof(struct cmsghdr)) / sizeof(int32_t);
        }

        struct cmsghdr* header = NULL;
        int32_t* output_fds = NULL;
        if (available_fds > 0)
        {
            header = (struct cmsghdr*)((uint8_t*)msg->msg_control + used);
            output_fds = (int32_t*)((uint8_t*)header + sizeof(struct cmsghdr));
        }

        uint32_t installed_count = 0;
        for (uint32_t i = 0; i < message->file_count; ++i)
        {
            if (installed_count < available_fds)
            {
                uint32_t descriptor_flags = (flags & MSG_CMSG_CLOEXEC) ? FD_CLOEXEC : 0;
                int32_t fd = process_install_file(g_current_thread->owner, message->files[i], descriptor_flags);
                if (fd >= 0)
                {
                    output_fds[installed_count++] = fd;
                    message->files[i] = NULL;
                    continue;
                }
            }

            msg->msg_flags |= MSG_CTRUNC;
        }

        if (installed_count > 0)
        {
            header->cmsg_level = SOL_SOCKET;
            header->cmsg_type = SCM_RIGHTS;
            header->cmsg_len = sizeof(struct cmsghdr) + installed_count * sizeof(int32_t);
            used += CMSG_ALIGN(header->cmsg_len);
        }

        socket_control_message_destroy(message);
    }

    msg->msg_controllen = used;
}

ssize_t syscall_sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
    if (msg == NULL ||
        !check_user_access((void*)msg) ||
        !check_user_access((uint8_t*)msg + sizeof(struct msghdr) - 1))
    {
        return -EFAULT;
    }

    if (msg->msg_iovlen > 1024 ||
        (msg->msg_iovlen > 0 &&
         (!check_user_access(msg->msg_iov) ||
          !check_user_access(msg->msg_iov + msg->msg_iovlen - 1))))
    {
        return -EFAULT;
    }

    for (uint32_t i = 0; i < msg->msg_iovlen; ++i)
    {
        struct iovec* iov = &msg->msg_iov[i];
        if (iov->iov_len > 0 &&
            (iov->iov_base == NULL ||
             !check_user_access(iov->iov_base) ||
             !check_user_access((uint8_t*)iov->iov_base + iov->iov_len - 1)))
        {
            return -EFAULT;
        }
    }

    int32_t error = -1;
    Socket* socket = get_socket(sockfd, &error);
    if (socket == NULL)
    {
        return error;
    }

    SocketControlMessage* control = socket_get_send_control(msg, &error);
    if (error < 0 && msg->msg_controllen > 0 && control == NULL)
    {
        return error;
    }

    if (control != NULL && socket->connection == NULL)
    {
        socket_control_message_destroy(control);
        return -ENOTCONN;
    }

    int32_t total = 0;
    uint64_t target_time = socket_timeout_deadline(socket->send_timeout_ms);

    if (socket->socket_send == NULL)
    {
        if (control != NULL)
        {
            socket_control_message_destroy(control);
        }
        return -EOPNOTSUPP;
    }

    for (uint32_t i = 0; i < msg->msg_iovlen; ++i)
    {
        struct iovec* iov = &msg->msg_iov[i];
        if (iov->iov_len == 0)
        {
            continue;
        }

        int32_t written = socket->socket_send(socket, sockfd, iov->iov_base, iov->iov_len, flags, target_time);
        if (written <= 0)
        {
            if (control != NULL)
            {
                socket_control_message_destroy(control);
            }
            return total > 0 ? total : written;
        }

        total += written;

        if (control != NULL)
        {
            control->byte_offset = socket->connection->bytes_enqueued - written;
            queue_enqueue(socket->connection->control_queue, control);
            control = NULL;
        }

        if ((uint32_t)written < iov->iov_len)
        {
            break;
        }
    }

    if (control != NULL)
    {
        socket_control_message_destroy(control);
    }

    return total;
}

ssize_t syscall_recvmsg(int sockfd, struct msghdr *msg, int flags)
{
    if (msg == NULL ||
        !check_user_access(msg) ||
        !check_user_access((uint8_t*)msg + sizeof(struct msghdr) - 1))
    {
        return -EFAULT;
    }

    if (flags & MSG_PEEK)
    {
        return -EOPNOTSUPP;
    }

    if (msg->msg_iovlen > 1024 ||
        (msg->msg_iovlen > 0 &&
         (!check_user_access(msg->msg_iov) ||
          !check_user_access(msg->msg_iov + msg->msg_iovlen - 1))) ||
        (msg->msg_controllen > 0 &&
         (msg->msg_control == NULL ||
          !check_user_access(msg->msg_control) ||
          !check_user_access((uint8_t*)msg->msg_control + msg->msg_controllen - 1))))
    {
        return -EFAULT;
    }

    for (uint32_t i = 0; i < msg->msg_iovlen; ++i)
    {
        struct iovec* iov = &msg->msg_iov[i];
        if (iov->iov_len > 0 &&
            (iov->iov_base == NULL ||
             !check_user_access(iov->iov_base) ||
             !check_user_access((uint8_t*)iov->iov_base + iov->iov_len - 1)))
        {
            return -EFAULT;
        }
    }

    int32_t error = -1;
    Socket* socket = get_socket(sockfd, &error);
    if (socket == NULL)
    {
        return error;
    }

    msg->msg_flags = 0;

    if (socket->socket_recv == NULL)
    {
        return -EOPNOTSUPP;
    }

    int32_t total = 0;
    uint64_t target_time = socket_timeout_deadline(socket->receive_timeout_ms);
    for (uint32_t i = 0; i < msg->msg_iovlen; ++i)
    {
        struct iovec* iov = &msg->msg_iov[i];
        if (iov->iov_len == 0)
        {
            continue;
        }

        if (total > 0 && fifobuffer_get_size(socket->buffer_in) == 0)
        {
            break;
        }

        int32_t read = socket->socket_recv(socket, sockfd, iov->iov_base, iov->iov_len, flags, target_time);
        if (read <= 0)
        {
            if (total == 0)
            {
                return read;
            }
            break;
        }

        total += read;

        if ((uint32_t)read < iov->iov_len)
        {
            break;
        }
    }

    if (total > 0)
    {
        socket_receive_control(socket, msg, flags);
    }

    return total;
}


static int32_t socket_timeout_from_timeval(int64_t seconds, int64_t microseconds, uint32_t* timeout_ms)
{
    if (seconds < 0 || microseconds < 0 || microseconds > 999999)
    {
        return -EINVAL;
    }

    uint32_t extra_ms = ((uint32_t)microseconds + 999) / 1000;
    if (seconds > (int64_t)((0xFFFFFFFFU - extra_ms) / 1000))
    {
        return -EDOM;
    }

    *timeout_ms = (uint32_t)seconds * 1000 + extra_ms;
    return 0;
}

static int32_t socket_get_timeout_value(int optname, const void* optval, socklen_t optlen, uint32_t* timeout_ms)
{
    if (optname == SO_RCVTIMEO_OLD || optname == SO_SNDTIMEO_OLD)
    {
        if (optlen < sizeof(struct timeval32))
        {
            return -EINVAL;
        }

        if (optval == NULL ||
            !check_user_access((void*)optval) ||
            !check_user_access((uint8_t*)optval + sizeof(struct timeval32) - 1))
        {
            return -EFAULT;
        }

        struct timeval32 value;
        memcpy((uint8_t*)&value, (const uint8_t*)optval, sizeof(value));
        return socket_timeout_from_timeval(value.tv_sec, value.tv_usec, timeout_ms);
    }

    if (optname == SO_RCVTIMEO_NEW || optname == SO_SNDTIMEO_NEW)
    {
        if (optlen < sizeof(struct timeval64))
        {
            return -EINVAL;
        }

        if (optval == NULL ||
            !check_user_access((void*)optval) ||
            !check_user_access((uint8_t*)optval + sizeof(struct timeval64) - 1))
        {
            return -EFAULT;
        }

        struct timeval64 value;
        memcpy((uint8_t*)&value, (const uint8_t*)optval, sizeof(value));
        return socket_timeout_from_timeval(value.tv_sec, value.tv_usec, timeout_ms);
    }

    return -ENOPROTOOPT;
}

static int32_t socket_put_timeout_value(int optname, void* optval, socklen_t* optlen, uint32_t timeout_ms)
{
    if (optlen == NULL ||
        !check_user_access(optlen) ||
        !check_user_access((uint8_t*)optlen + sizeof(socklen_t) - 1))
    {
        return -EFAULT;
    }

    uint32_t value_size;
    uint8_t value[sizeof(struct timeval64)];
    if (optname == SO_RCVTIMEO_OLD || optname == SO_SNDTIMEO_OLD)
    {
        struct timeval32 timeout;
        timeout.tv_sec = (int32_t)(timeout_ms / 1000);
        timeout.tv_usec = (int32_t)((timeout_ms % 1000) * 1000);
        value_size = sizeof(timeout);
        memcpy(value, (const uint8_t*)&timeout, value_size);
    }
    else if (optname == SO_RCVTIMEO_NEW || optname == SO_SNDTIMEO_NEW)
    {
        struct timeval64 timeout;
        timeout.tv_sec = (int64_t)(timeout_ms / 1000);
        timeout.tv_usec = (int64_t)((timeout_ms % 1000) * 1000);
        value_size = sizeof(timeout);
        memcpy(value, (const uint8_t*)&timeout, value_size);
    }
    else
    {
        return -ENOPROTOOPT;
    }

    uint32_t copy_size = MIN(*optlen, value_size);
    if (copy_size > 0)
    {
        if (optval == NULL ||
            !check_user_access(optval) ||
            !check_user_access((uint8_t*)optval + copy_size - 1))
        {
            return -EFAULT;
        }

        memcpy((uint8_t*)optval, value, copy_size);
    }

    *optlen = copy_size;
    return 0;
}

int syscall_getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
    int32_t error = -1;
    Socket* socket = get_socket(sockfd, &error);
    if (socket == NULL)
    {
        return error;
    }

    if (level != SOL_SOCKET)
    {
        return -ENOPROTOOPT;
    }

    uint32_t timeout_ms;
    if (optname == SO_RCVTIMEO_OLD || optname == SO_RCVTIMEO_NEW)
    {
        timeout_ms = socket->receive_timeout_ms;
    }
    else if (optname == SO_SNDTIMEO_OLD || optname == SO_SNDTIMEO_NEW)
    {
        timeout_ms = socket->send_timeout_ms;
    }
    else
    {
        return -ENOPROTOOPT;
    }

    return socket_put_timeout_value(optname, optval, optlen, timeout_ms);
}

int syscall_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
    int32_t error = -1;
    Socket* socket = get_socket(sockfd, &error);
    if (socket == NULL)
    {
        return error;
    }

    if (level != SOL_SOCKET)
    {
        return -ENOPROTOOPT;
    }

    uint32_t timeout_ms;
    error = socket_get_timeout_value(optname, optval, optlen, &timeout_ms);
    if (error < 0)
    {
        return error;
    }

    if (optname == SO_RCVTIMEO_OLD || optname == SO_RCVTIMEO_NEW)
    {
        socket->receive_timeout_ms = timeout_ms;
    }
    else
    {
        socket->send_timeout_ms = timeout_ms;
    }

    return 0;
}
