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
    void* privateData;
    int32 controllingProcess;
    int32 foregroundProcess;
    winsize_t winsize;
    FifoBuffer* bufferMasterWrite;
    Spinlock bufferMasterWriteLock;
    FifoBuffer* bufferMasterRead;
    Spinlock bufferMasterReadLock;
    FifoBuffer* bufferEcho; //used in only echoing by master_write, no need lock
    List* slaveReaders;
    Spinlock slaveReadersLock;
    Thread* masterReader;
    TtyIOReady masterReadReady; //used for kernel terminal, because it does not read like a user process
    uint8 lineBuffer[TTYDEV_LINEBUFFER_SIZE];
    uint32 lineBufferIndex;
    struct termios term;

} TtyDev;

FileSystemNode* createTTYDev();

int32 TtyDev_master_read_nonblock(File *file, uint32 size, uint8 *buffer);

#endif //TTYDEV_H