#ifndef TTYDEV_H
#define TTYDEV_H

#include "common.h"
#include "spinlock.h"
#include "termios.h"
#include "fs.h"

#define TTYDEV_LINEBUFFER_SIZE 4096

typedef struct FileSystemNode FileSystemNode;
typedef struct FifoBuffer FifoBuffer;
typedef struct List List;
typedef struct Thread Thread;

typedef struct winsize_t
{
    uint16 ws_row;	/* rows, in characters */
    uint16 ws_col;	/* columns, in characters */
    uint16 ws_xpixel;	/* horizontal size, pixels */
    uint16 ws_ypixel;	/* vertical size, pixels */
} winsize_t;

typedef struct TtyDev TtyDev;

typedef void (*TtyIOReady)(TtyDev* tty, uint32 size);
typedef struct TtyDev
{
    FileSystemNode* master_node;
    FileSystemNode* slave_node;
    void* private_data;
    int32 controlling_process;
    int32 foreground_process;
    winsize_t winsize;
    FifoBuffer* buffer_master_write;
    Spinlock buffer_master_write_lock;
    FifoBuffer* buffer_master_read;
    Spinlock buffer_master_read_lock;
    FifoBuffer* buffer_echo; //used in only echoing by master_write, no need lock
    List* slave_readers;
    Spinlock slave_readers_lock;
    Thread* master_reader;
    TtyIOReady master_read_ready; //used for kernel terminal, because it does not read like a user process
    uint8 line_buffer[TTYDEV_LINEBUFFER_SIZE];
    uint32 line_buffer_index;
    struct termios term;

} TtyDev;

FileSystemNode* ttydev_create();

int32 ttydev_master_read_nonblock(File *file, uint32 size, uint8 *buffer);

#endif //TTYDEV_H