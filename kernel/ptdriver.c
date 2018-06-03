#include "ptdriver.h"
#include "fifobuffer.h"
#include "list.h"
#include "spinlock.h"
#include "devfs.h"
#include "fs.h"
#include "alloc.h"

typedef struct PtDevice
{
    FifoBuffer* bufferMasterWrite;
    FifoBuffer* bufferSlaveWrite;
    FileSystemNode* masterNode;
    FileSystemNode* slaveNode;
    List* accessingThreadsSlave;
    Spinlock accessingThreadsSlaveLock;
} PtDevice;

static List* gPtList = NULL;
static Spinlock gPtListLock;

static uint32 gIdGenerator = 1;

static int32 ptmaster_read(File *file, uint32 size, uint8 *buffer);
static int32 ptmaster_write(File *file, uint32 size, uint8 *buffer);

static BOOL ptslave_open(File *file, uint32 flags);
static void ptslave_close(File *file);
static int32 ptslave_read(File *file, uint32 size, uint8 *buffer);
static int32 ptslave_write(File *file, uint32 size, uint8 *buffer);

void initializePseudoTerminal()
{
    gPtList = List_Create();

    Spinlock_Init(&gPtListLock);
}

int getSlavePath(FileSystemNode* masterNode, char* buffer, uint32 bufferSize)
{
    PtDevice* device = (PtDevice*)masterNode->privateNodeData;

    return getFileSystemNodePath(device->slaveNode, buffer, bufferSize);
}

FileSystemNode* createPseudoTerminal()
{
    PtDevice* device = kmalloc(sizeof(PtDevice));
    memset((uint8*)device, 0, sizeof(PtDevice));

    Device pts;
    memset((uint8*)&pts, 0, sizeof(Device));
    sprintf(pts.name, "pts%d", gIdGenerator++);
    pts.privateData = device;
    pts.open = ptslave_open;
    pts.close = ptslave_close;
    pts.read = ptslave_read;
    pts.write = ptslave_write;

    device->slaveNode = registerDevice(&pts);

    if (NULL == device->slaveNode)
    {
        kfree(device);

        return NULL;
    }

    FileSystemNode* masterNode = kmalloc(sizeof(FileSystemNode));
    memset((uint8*)masterNode, 0, sizeof(FileSystemNode));
    masterNode->privateNodeData = device;
    masterNode->read = ptmaster_read;
    masterNode->write = ptmaster_write;

    device->masterNode = masterNode;
    device->bufferMasterWrite = FifoBuffer_create(32);
    device->bufferSlaveWrite = FifoBuffer_create(32);
    device->accessingThreadsSlave = List_Create();
    Spinlock_Init(&device->accessingThreadsSlaveLock);




    Spinlock_Lock(&gPtListLock);
    List_Append(gPtList, device);
    Spinlock_Unlock(&gPtListLock);

    return masterNode;
}

static void blockAccessingThreads(PtDevice* ptDevice)
{
    disableInterrupts();

    List_Foreach (n, ptDevice->accessingThreadsSlave)
    {
        Thread* reader = n->data;

        reader->state = TS_WAITIO;

        reader->state_privateData = ptDevice;
    }

    enableInterrupts();

    halt();
}

//must be called with interrupts disabled
static void wakeupAccessingThreads(PtDevice* ptDevice)
{
    List_Foreach (n, ptDevice->accessingThreadsSlave)
    {
        Thread* reader = n->data;

        if (reader->state == TS_WAITIO)
        {
            if (reader->state_privateData == ptDevice)
            {
                reader->state = TS_RUN;
            }
        }
    }
}

static int32 ptmaster_read(File *file, uint32 size, uint8 *buffer)
{
    if (0 == size || NULL == buffer)
    {
        return -1;
    }

    PtDevice* device = (PtDevice*)file->node->privateNodeData;

    uint32 used = 0;
    while ((used = FifoBuffer_getSize(device->bufferSlaveWrite)) < size)
    {
        blockAccessingThreads(device);
    }

    disableInterrupts();

    int32 readBytes = FifoBuffer_dequeue(device->bufferSlaveWrite, buffer, size);

    wakeupAccessingThreads(device);

    return readBytes;
}

static int32 ptmaster_write(File *file, uint32 size, uint8 *buffer)
{
    if (0 == size || NULL == buffer)
    {
        return -1;
    }

    PtDevice* device = (PtDevice*)file->node->privateNodeData;

    uint32 free = 0;
    while ((free = FifoBuffer_getFree(device->bufferMasterWrite)) < size)
    {
        blockAccessingThreads(device);
    }

    disableInterrupts();

    int32 bytesWritten = FifoBuffer_enqueue(device->bufferMasterWrite, buffer, size);

    wakeupAccessingThreads(device);

    return bytesWritten;
}

static BOOL ptslave_open(File *file, uint32 flags)
{
    PtDevice* device = (PtDevice*)file->node->privateNodeData;

    Spinlock_Lock(&device->accessingThreadsSlaveLock);

    List_Append(device->accessingThreadsSlave, file->thread);

    Spinlock_Unlock(&device->accessingThreadsSlaveLock);

    return TRUE;
}

static void ptslave_close(File *file)
{
    PtDevice* device = (PtDevice*)file->node->privateNodeData;

    Spinlock_Lock(&device->accessingThreadsSlaveLock);

    List_RemoveFirstOccurrence(device->accessingThreadsSlave, file->thread);

    Spinlock_Unlock(&device->accessingThreadsSlaveLock);
}

static int32 ptslave_read(File *file, uint32 size, uint8 *buffer)
{
    if (0 == size || NULL == buffer)
    {
        return -1;
    }

    PtDevice* device = (PtDevice*)file->node->privateNodeData;

    uint32 used = 0;
    while ((used = FifoBuffer_getSize(device->bufferMasterWrite)) < size)
    {
        blockAccessingThreads(device);
    }

    disableInterrupts();

    int32 readBytes = FifoBuffer_dequeue(device->bufferMasterWrite, buffer, size);

    wakeupAccessingThreads(device);

    return readBytes;
}

static int32 ptslave_write(File *file, uint32 size, uint8 *buffer)
{
    if (0 == size || NULL == buffer)
    {
        return -1;
    }

    PtDevice* device = (PtDevice*)file->node->privateNodeData;

    uint32 free = 0;
    while ((free = FifoBuffer_getFree(device->bufferSlaveWrite)) < size)
    {
        blockAccessingThreads(device);
    }

    disableInterrupts();

    int32 bytesWritten = FifoBuffer_enqueue(device->bufferSlaveWrite, buffer, size);

    wakeupAccessingThreads(device);

    return bytesWritten;
}
