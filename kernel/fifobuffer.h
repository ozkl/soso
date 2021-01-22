#ifndef FIFOBUFFER_H
#define FIFOBUFFER_H

#include "common.h"

typedef struct FifoBuffer
{
    uint8_t* data;
    uint32_t write_index;
    uint32_t read_index;
    uint32_t capacity;
    uint32_t used_bytes;
} FifoBuffer;

FifoBuffer* fifobuffer_create(uint32_t capacity);
void fifobuffer_destroy(FifoBuffer* fifo_buffer);
void fifobuffer_clear(FifoBuffer* fifo_buffer);
BOOL fifobuffer_is_empty(FifoBuffer* fifo_buffer);
uint32_t fifobuffer_get_size(FifoBuffer* fifo_buffer);
uint32_t fifobuffer_get_capacity(FifoBuffer* fifo_buffer);
uint32_t fifobuffer_get_free(FifoBuffer* fifo_buffer);
int32_t fifobuffer_enqueue(FifoBuffer* fifo_buffer, uint8_t* data, uint32_t size);
int32_t fifobuffer_dequeue(FifoBuffer* fifo_buffer, uint8_t* data, uint32_t size);
int32_t fifobuffer_enqueue_from_other(FifoBuffer* fifo_buffer, FifoBuffer* other);

#endif // FIFOBUFFER_H
