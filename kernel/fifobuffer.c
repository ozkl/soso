#include "fifobuffer.h"
#include "alloc.h"

typedef struct FifoBufferElement
{
    uint8* data;
    uint32 bytes;
    uint32 bytesRead;
} FifoBufferElement;

typedef struct FifoBuffer
{
    FifoBufferElement* elements;
    uint32 writeIndex;
    uint32 readIndex;
    uint32 elementCapacity;
} FifoBuffer;

FifoBuffer* FifoBuffer_create(uint32 elementCapacity)
{
    FifoBuffer* fifo = kmalloc(sizeof(FifoBuffer));
    memset((uint8*)fifo, 0, sizeof(FifoBuffer));
    fifo->elements = kmalloc(elementCapacity * sizeof(FifoBufferElement));
    memset((uint8*)fifo->elements, 0, elementCapacity * sizeof(FifoBufferElement));
    fifo->elementCapacity = elementCapacity;

    return fifo;
}

void FifoBuffer_destroy(FifoBuffer* fifoBuffer)
{
    kfree(fifoBuffer->elements);
    kfree(fifoBuffer);
}

BOOL FifoBuffer_isEmpty(FifoBuffer* fifoBuffer)
{
    if (fifoBuffer->readIndex == fifoBuffer->writeIndex)
    {
        return TRUE;
    }

    return FALSE;
}

uint32 FifoBuffer_enqueue(FifoBuffer* fifoBuffer, uint8* data, uint8 size)
{
    if (size == 0)
    {
        return 0;
    }

    if (((fifoBuffer->writeIndex + 1) % fifoBuffer->elementCapacity) == fifoBuffer->readIndex)
    {
        //Buffer is full
        return 0;
    }

    kfree(fifoBuffer->elements[fifoBuffer->writeIndex].data);
    fifoBuffer->elements[fifoBuffer->writeIndex].data = kmalloc(size);
    memcpy(fifoBuffer->elements[fifoBuffer->writeIndex].data, data, size);
    fifoBuffer->elements[fifoBuffer->writeIndex].bytesRead = 0;
    fifoBuffer->elements[fifoBuffer->writeIndex].bytes = size;

    fifoBuffer->writeIndex++;
    fifoBuffer->writeIndex %= fifoBuffer->elementCapacity;

    return size;
}

uint32 FifoBuffer_dequeue(FifoBuffer* fifoBuffer, uint8* data, uint8 size)
{
    if (size == 0)
    {
        return 0;
    }

    if (fifoBuffer->writeIndex == fifoBuffer->readIndex)
    {
        //Buffer is empty
        return 0;
    }

    int bytesReadPreviously = fifoBuffer->elements[fifoBuffer->readIndex].bytesRead;

    int toReadBytes = size;
    if (bytesReadPreviously + toReadBytes > fifoBuffer->elements[fifoBuffer->readIndex].bytes)
    {
        toReadBytes = fifoBuffer->elements[fifoBuffer->readIndex].bytes - bytesReadPreviously;
    }

    memcpy(data, fifoBuffer->elements[fifoBuffer->readIndex].data + bytesReadPreviously, toReadBytes);

    fifoBuffer->elements[fifoBuffer->readIndex].bytesRead += toReadBytes;

    if (fifoBuffer->elements[fifoBuffer->readIndex].bytesRead == fifoBuffer->elements[fifoBuffer->readIndex].bytes)
    {
        //Read of this element finished, cleaning
        kfree(fifoBuffer->elements[fifoBuffer->readIndex].data);
        fifoBuffer->elements[fifoBuffer->readIndex].data = NULL;
        fifoBuffer->elements[fifoBuffer->readIndex].bytes = 0;
        fifoBuffer->elements[fifoBuffer->readIndex].bytesRead = 0;

        fifoBuffer->readIndex++;
        fifoBuffer->readIndex %= fifoBuffer->elementCapacity;
    }

    return toReadBytes;
}
