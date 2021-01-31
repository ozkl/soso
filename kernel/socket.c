#include "socket.h"
#include "unixsocket.h"
#include "errno.h"
#include "common.h"
#include "process.h"
#include "list.h"
#include "fs.h"
#include "alloc.h"

#define DOMAIN_SIZE 3

List* g_socket_list = NULL;


static SocketFunction g_domain_setup_functions[DOMAIN_SIZE];

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
            File* file = process->fd[sockfd];

            if (file)
            {
                if (file->node->node_type != FT_SOCKET)
                {
                    *error = -ENOTSOCK;
                    return NULL;
                }

                Socket* socket = (Socket*)file->node->private_node_data;

                socket->last_thread = g_current_thread;

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

    list_append(g_socket_list, socket);

    return socket;
}

static void socket_destroy(Socket* socket)
{
    list_remove_first_occurrence(g_socket_list, socket);

    fifobuffer_destroy(socket->buffer_out);
    fifobuffer_destroy(socket->buffer_in);

    queue_destroy(socket->accept_queue);

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

static void socket_fs_close(File* file)
{
    Socket* socket = (Socket*)file->node->private_node_data;

    if (socket->socket_closing)
    {
        socket->socket_closing(socket);
    }

    if (socket->connection)
    {
        //disconnect other end as well

        socket->connection->connection = NULL;
        socket->connection->disconnected = TRUE;
    }

    kfree(socket->node);
    socket->node = NULL;

    socket_destroy(socket);
}

int syscall_socket(int domain, int type, int protocol)
{
    //printkf("socket %d %d %d\n", domain, type, protocol);

    if (domain >= 0 && domain < DOMAIN_SIZE)
    {
        Socket* socket = socket_create();

        socket->domain = domain;

        FileSystemNode* node = (FileSystemNode*)kmalloc(sizeof(FileSystemNode));
        memset((uint8_t*)node, 0, sizeof(FileSystemNode));

        socket->last_thread = g_current_thread;

        socket->node = node;
        node->private_node_data = socket;

        node->node_type = FT_SOCKET;

        node->open = socket_fs_open;
        node->close = socket_fs_close;

        File* file = fs_open_for_process(g_current_thread, node, O_RDWR);

        if (file)
        {
            return file->fd;
        }
    }

    return -EPERM;
}

int syscall_socketpair(int domain, int type, int protocol, int sv[2])
{
    return -EPERM;
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
    return -EPERM;
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
        return socket->socket_send(socket, sockfd, buf, len, flags);
    }

    return error;
}

ssize_t syscall_recv(int sockfd, void *buf, size_t len, int flags)
{
    int error = -1;
    Socket* socket = get_socket(sockfd, &error);

    if (socket && socket->socket_recv)
    {
        return socket->socket_recv(socket, sockfd, buf, len, flags);
    }

    return error;
}

ssize_t syscall_sendto(int sockfd, const void *buf, size_t len, int flags,
                      const struct sockaddr *dest_addr, socklen_t addrlen)
{
    return -EPERM;
}

ssize_t syscall_recvfrom(int sockfd, void *buf, size_t len, int flags,
                        struct sockaddr *src_addr, socklen_t *addrlen)
{
    return -EPERM;
}

ssize_t syscall_sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
    return -EPERM;
}

ssize_t syscall_recvmsg(int sockfd, struct msghdr *msg, int flags)
{
    return -EPERM;
}


int syscall_getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
    return -EPERM;
}

int syscall_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
    return -EPERM;
}
