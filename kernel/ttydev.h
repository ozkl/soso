#ifndef TTYDEV_H
#define TTYDEV_H

#include "common.h"
#include "fs.h"
#include "termios.h"

#define TTYDEV_LINEBUFFER_SIZE 128

typedef struct TtyDev
{
    FileSystemNode* masterNode;
    FileSystemNode* slaveNode;
    void* privateData;
    uint8 lineBuffer[TTYDEV_LINEBUFFER_SIZE];
    uint32 lineBufferIndex;
    struct termios term;

} TtyDev;

FileSystemNode* createTTYDev();

#endif //TTYDEV_H