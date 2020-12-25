#include "alloc.h"
#include "fs.h"
#include "devfs.h"
#include "device.h"
#include "fifobuffer.h"
#include "list.h"
#include "ttydev.h"

static BOOL master_open(File *file, uint32 flags);
static void master_close(File *file);
static int32 master_read(File *file, uint32 size, uint8 *buffer);
static int32 master_write(File *file, uint32 size, uint8 *buffer);

static BOOL slave_open(File *file, uint32 flags);
static void slave_close(File *file);
static int32 slave_read(File *file, uint32 size, uint8 *buffer);
static int32 slave_write(File *file, uint32 size, uint8 *buffer);

static uint32 gNameGenerator = 0;

FileSystemNode* createTTYDev()
{
    TtyDev* ttyDev = kmalloc(sizeof(TtyDev));
    memset((uint8*)ttyDev, 0, sizeof(TtyDev));

    ttyDev->term.c_cc[VMIN] = 1;
    ttyDev->term.c_lflag |= ECHO;
    ttyDev->term.c_lflag |= ICANON;

    ttyDev->bufferMasterWrite = FifoBuffer_create(1024);
    ttyDev->bufferMasterRead = FifoBuffer_create(1024);
    ttyDev->slaveReaders = List_Create();

    
    Spinlock_Init(&ttyDev->bufferMasterWriteLock);
    Spinlock_Init(&ttyDev->bufferMasterReadLock);
    Spinlock_Init(&ttyDev->slaveReadersLock);

    ++gNameGenerator;

    Device master;
    memset((uint8*)&master, 0, sizeof(Device));
    sprintf(master.name, "ptty%d-m", gNameGenerator);
    master.deviceType = FT_CharacterDevice;
    master.open = master_open;
    master.close = master_close;
    master.read = master_read;
    master.write = master_write;
    master.privateData = ttyDev;

    Device slave;
    memset((uint8*)&slave, 0, sizeof(Device));
    sprintf(slave.name, "ptty%d", gNameGenerator);
    slave.deviceType = FT_CharacterDevice;
    slave.open = slave_open;
    slave.close = slave_close;
    slave.read = slave_read;
    slave.write = slave_write;
    slave.privateData = ttyDev;

    FileSystemNode* masterNode = registerDevice(&master);
    FileSystemNode* slaveNode = registerDevice(&slave);

    ttyDev->masterNode = masterNode;
    ttyDev->slaveNode = slaveNode;

    return masterNode;
}

static void wakeSlaveReaders(TtyDev* tty)
{
    Spinlock_Lock(&tty->slaveReadersLock);
    List_Foreach(n, tty->slaveReaders)
    {
        Thread* thread = (Thread*)n->data;
        if (thread->state == TS_WAITIO && thread->state_privateData == tty)
        {
            resumeThread(thread);
        }
    }
    Spinlock_Unlock(&tty->slaveReadersLock);
}

static BOOL master_open(File *file, uint32 flags)
{
    return TRUE;
}

static void master_close(File *file)
{
    
}

static int32 master_read(File *file, uint32 size, uint8 *buffer)
{
    enableInterrupts();

    if (size > 0)
    {
        TtyDev* tty = (TtyDev*)file->node->privateNodeData;

        
        //while (TRUE)
        {
            Spinlock_Lock(&tty->bufferMasterReadLock);

            uint32 neededSize = tty->term.c_cc[VMIN];
            uint32 bufferLen = FifoBuffer_getSize(tty->bufferMasterRead);

            int readSize = -1;

            if (bufferLen >= neededSize)
            {
                readSize = FifoBuffer_dequeue(tty->bufferMasterRead, buffer, MIN(bufferLen, size));
            }

            Spinlock_Unlock(&tty->bufferMasterReadLock);

            if (readSize > 0)
            {
                return readSize;
            }

            //tty->masterReader = file->thread;
            //changeThreadState(file->thread, TS_WAITIO, tty);
            //halt();
        }
    }

    return -1;
}

static int32 master_write(File *file, uint32 size, uint8 *buffer)
{
    if (size == 0)
    {
        return -1;
    }

    TtyDev* tty = (TtyDev*)file->node->privateNodeData;

    enableInterrupts();

    Spinlock_Lock(&tty->bufferMasterWriteLock);

    uint32 free = FifoBuffer_getFree(tty->bufferMasterWrite);

    uint32 toWrite = MIN(size, free);

    uint32 written = 0;

    if ((tty->term.c_lflag & ICANON) == ICANON)
    {
        for (uint32 k = 0; k < toWrite; ++k)
        {
            uint8 character = buffer[k];

            if (tty->lineBufferIndex >= TTYDEV_LINEBUFFER_SIZE - 1)
            {
                tty->lineBufferIndex = 0;
            }

            if (character == '\b')
            {
                if (tty->lineBufferIndex > 0)
                {
                    tty->lineBuffer[--tty->lineBufferIndex] = '\0';
                }
            }
            else if (character == '\r')
            {
                int bytesToCopy = tty->lineBufferIndex;

                if (bytesToCopy >= tty->term.c_cc[VMIN])
                {
                    tty->lineBufferIndex = 0;
                    written = FifoBuffer_enqueue(tty->bufferMasterWrite, tty->lineBuffer, bytesToCopy);
                    written += FifoBuffer_enqueue(tty->bufferMasterWrite, "\n", 1);

                    if (written > 0)
                    {
                        wakeSlaveReaders(tty);
                    }
                }
            }
            else
            {
                tty->lineBuffer[tty->lineBufferIndex++] = character;
            }
        }
    }
    else
    {
        written = FifoBuffer_enqueue(tty->bufferMasterWrite, buffer, toWrite);

        if (written > 0)
        {
            wakeSlaveReaders(tty);
        }
    }
    
    Spinlock_Unlock(&tty->bufferMasterWriteLock);

    if ((tty->term.c_lflag & ECHO) == ECHO)
    {
        Spinlock_Lock(&tty->bufferMasterReadLock);

        FifoBuffer_enqueue(tty->bufferMasterRead, buffer, toWrite);

        Spinlock_Unlock(&tty->bufferMasterReadLock);
    }


    return (int32)written;
}

static BOOL slave_open(File *file, uint32 flags)
{
    return TRUE;
}

static void slave_close(File *file)
{
    
}

static int32 slave_read(File *file, uint32 size, uint8 *buffer)
{
    enableInterrupts();

    if (size > 0)
    {
        TtyDev* tty = (TtyDev*)file->node->privateNodeData;

        
        while (TRUE)
        {
            Spinlock_Lock(&tty->bufferMasterWriteLock);

            uint32 neededSize = tty->term.c_cc[VMIN];
            uint32 bufferLen = FifoBuffer_getSize(tty->bufferMasterWrite);

            int readSize = -1;

            if (bufferLen >= neededSize)
            {
                readSize = FifoBuffer_dequeue(tty->bufferMasterWrite, buffer, MIN(bufferLen, size));
            }

            Spinlock_Unlock(&tty->bufferMasterWriteLock);

            if (readSize > 0)
            {
                return readSize;
            }

            //TODO: remove reader from list
            Spinlock_Lock(&tty->slaveReadersLock);
            List_RemoveFirstOccurrence(tty->slaveReaders, file->thread);
            List_Append(tty->slaveReaders, file->thread);
            Spinlock_Unlock(&tty->slaveReadersLock);

            changeThreadState(file->thread, TS_WAITIO, tty);
            halt();
        }
    }

    return -1;
}

static int32 slave_write(File *file, uint32 size, uint8 *buffer)
{
    if (size == 0)
    {
        return -1;
    }

    TtyDev* tty = (TtyDev*)file->node->privateNodeData;

    enableInterrupts();

    //while (TRUE)
    {
        Spinlock_Lock(&tty->bufferMasterReadLock);

        uint32 free = FifoBuffer_getFree(tty->bufferMasterRead);

        int written = 0;

        if (free > 0)
        {
            written = FifoBuffer_enqueue(tty->bufferMasterRead, buffer, MIN(size, free));
        }

        Spinlock_Unlock(&tty->bufferMasterReadLock);

        if (written > 0)
        {
            if (tty->masterReader)
            {
                if (tty->masterReader->state == TS_WAITIO && tty->masterReader->state_privateData == tty)
                {
                    resumeThread(tty->masterReader);
                }
            }
            return written;
        }
    }

    return -1;
}