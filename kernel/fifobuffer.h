#ifndef FIFOBUFFER_H
#define FIFOBUFFER_H

#include "common.h"

typedef struct FifoBuffer
{
    uint8* data;
    uint32 write_index;
    uint32 read_index;
    uint32 capacity;
    uint32 used_bytes;
} FifoBuffer;

FifoBuffer* fifobuffer_create(uint32 capacity);
void fifobuffer_destroy(FifoBuffer* fifo_buffer);
void fifobuffer_clear(FifoBuffer* fifo_buffer);
BOOL fifobuffer_is_empty(FifoBuffer* fifo_buffer);
uint32 fifobuffer_get_size(FifoBuffer* fifo_buffer);
uint32 fifobuffer_get_capacity(FifoBuffer* fifo_buffer);
uint32 fifobuffer_get_free(FifoBuffer* fifo_buffer);
int32 fifobuffer_enqueue(FifoBuffer* fifo_buffer, uint8* data, uint32 size);
int32 fifobuffer_dequeue(FifoBuffer* fifo_buffer, uint8* data, uint32 size);
int32 fifobuffer_enqueue_from_other(FifoBuffer* fifo_buffer, FifoBuffer* other);

#endif // FIFOBUFFER_H
