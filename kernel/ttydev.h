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
    uint16_t ws_row;	/* rows, in characters */
    uint16_t ws_col;	/* columns, in characters */
    uint16_t ws_xpixel;	/* horizontal size, pixels */
    uint16_t ws_ypixel;	/* vertical size, pixels */
} winsize_t;

typedef struct TtyDev TtyDev;

typedef void (*TtyIOReady)(TtyDev* tty, uint32_t size);
typedef struct TtyDev
{
    FileSystemNode* master_node;
    FileSystemNode* slave_node;
    void* private_data;
    int32_t controlling_process;
    int32_t foreground_process;
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
    uint8_t line_buffer[TTYDEV_LINEBUFFER_SIZE];
    uint32_t line_buffer_index;
    struct termios term;

} TtyDev;

FileSystemNode* ttydev_create();

int32_t ttydev_master_read_nonblock(File *file, uint32_t size, uint8_t *buffer);

#endif //TTYDEV_H