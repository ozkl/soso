#include "unixsocket.h"
#include "fifobuffer.h"
#include "list.h"
#include "fs.h"
#include "alloc.h"
#include "common.h"
#include "socket.h"

typedef struct
{
    char name[SOCKET_NAME_SIZE];
    Socket* owner;
} UnixSocket;

static void unixsocket_closing(Socket* socket);
static int unixsocket_bind(Socket* socket, int sockfd, const struct sockaddr *addr, socklen_t addrlen);

void unixsocket_setup(Socket* socket)
{
    printkf("unixsocket_setup\n");

    UnixSocket* unix_socket = (UnixSocket*)kmalloc(sizeof(UnixSocket));
    memset((uint8_t*)unix_socket, 0, sizeof(UnixSocket));

    socket->custom_socket = unix_socket;
    unix_socket->owner = socket;

    socket->socket_closing = unixsocket_closing;
    socket->socket_bind = unixsocket_bind;
}

static int unixsocket_bind(Socket* socket, int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    printkf("unixsocket_bind\n");

    return -1;
}

static void unixsocket_closing(Socket* socket)
{
    printkf("unixsocket_closing\n");

    UnixSocket* unix_socket = (UnixSocket*)socket->custom_socket;

    kfree(unix_socket);

    socket->custom_socket = NULL;
}