#ifndef TTYDEV_H
#define TTYDEV_H

#include "common.h"
#include "spinlock.h"
#include "termios.h"

#define TTYDEV_LINEBUFFER_SIZE 4096

typedef struct FileSystemNode FileSystemNode;
typedef struct FifoBuffer FifoBuffer;
typedef struct List List;
typedef struct Thread Thread;

typedef struct TtyDev
{
    FileSystemNode* masterNode;
    FileSystemNode* slaveNode;
    void* privateData;
    FifoBuffer* bufferMasterWrite;
    Spinlock bufferMasterWriteLock;
    FifoBuffer* bufferMasterRead;
    Spinlock bufferMasterReadLock;
    List* slaveReaders;
    Spinlock slaveReadersLock;
    Thread* masterReader;
    uint8 lineBuffer[TTYDEV_LINEBUFFER_SIZE];
    uint32 lineBufferIndex;
    struct termios term;

} TtyDev;

FileSystemNode* createTTYDev();

#endif //TTYDEV_H