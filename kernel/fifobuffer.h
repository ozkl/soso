#ifndef FIFOBUFFER_H
#define FIFOBUFFER_H

#include "common.h"

typedef struct FifoBuffer FifoBuffer;

FifoBuffer* FifoBuffer_create(uint32 elementCapacity);
void FifoBuffer_destroy(FifoBuffer* fifoBuffer);
BOOL FifoBuffer_isEmpty(FifoBuffer* fifoBuffer);
uint32 FifoBuffer_enqueue(FifoBuffer* fifoBuffer, uint8* data, uint8 size);
uint32 FifoBuffer_dequeue(FifoBuffer* fifoBuffer, uint8* data, uint8 size);

#endif // FIFOBUFFER_H
