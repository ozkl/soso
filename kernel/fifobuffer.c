#include "fifobuffer.h"
#include "alloc.h"

FifoBuffer* fifobuffer_create(uint32_t capacity)
{
    FifoBuffer* fifo = (FifoBuffer*)kmalloc(sizeof(FifoBuffer));
    memset((uint8_t*)fifo, 0, sizeof(FifoBuffer));
    fifo->data = (uint8_t*)kmalloc(capacity);
    memset((uint8_t*)fifo->data, 0, capacity);
    fifo->capacity= capacity;

    return fifo;
}

void fifobuffer_destroy(FifoBuffer* fifo_buffer)
{
    kfree(fifo_buffer->data);
    kfree(fifo_buffer);
}

void fifobuffer_clear(FifoBuffer* fifo_buffer)
{
    fifo_buffer->used_bytes = 0;
    fifo_buffer->read_index = 0;
    fifo_buffer->write_index = 0;
}

BOOL fifobuffer_is_empty(FifoBuffer* fifo_buffer)
{
    if (0 == fifo_buffer->used_bytes)
    {
        return TRUE;
    }

    return FALSE;
}

uint32_t fifobuffer_get_size(FifoBuffer* fifo_buffer)
{
    return fifo_buffer->used_bytes;
}

uint32_t fifobuffer_get_capacity(FifoBuffer* fifo_buffer)
{
    return fifo_buffer->capacity;
}

uint32_t fifobuffer_get_free(FifoBuffer* fifo_buffer)
{
    return fifo_buffer->capacity - fifo_buffer->used_bytes;
}

int32_t fifobuffer_enqueue(FifoBuffer* fifo_buffer, uint8_t* data, uint32_t size)
{
    if (size == 0)
    {
        return -1;
    }

    uint32_t bytes_available = fifo_buffer->capacity - fifo_buffer->used_bytes;

    uint32_t i = 0;
    while (fifo_buffer->used_bytes < fifo_buffer->capacity && i < size)
    {
        fifo_buffer->data[fifo_buffer->write_index] = data[i++];
        fifo_buffer->used_bytes++;
        fifo_buffer->write_index++;
        fifo_buffer->write_index %= fifo_buffer->capacity;
    }

    return (int32_t)i;
}

int32_t fifobuffer_dequeue(FifoBuffer* fifo_buffer, uint8_t* data, uint32_t size)
{
    if (size == 0)
    {
        return -1;
    }

    if (0 == fifo_buffer->used_bytes)
    {
        //Buffer is empty
        return 0;
    }

    uint32_t i = 0;
    while (fifo_buffer->used_bytes > 0 && i < size)
    {
        data[i++] = fifo_buffer->data[fifo_buffer->read_index];
        fifo_buffer->used_bytes--;
        fifo_buffer->read_index++;
        fifo_buffer->read_index %= fifo_buffer->capacity;
    }

    return i;
}

int32_t fifobuffer_enqueue_from_other(FifoBuffer* fifo_buffer, FifoBuffer* other)
{
    uint32_t otherSize = fifobuffer_get_size(other);

    if (otherSize == 0)
    {
        return 0;
    }

    uint32_t bytes_available = fifo_buffer->capacity - fifo_buffer->used_bytes;

    uint32_t i = 0;
    while (fifo_buffer->used_bytes < fifo_buffer->capacity && other->used_bytes > 0)
    {
        fifo_buffer->data[fifo_buffer->write_index] = other->data[other->read_index];
        fifo_buffer->used_bytes++;
        fifo_buffer->write_index++;
        fifo_buffer->write_index %= fifo_buffer->capacity;

        other->used_bytes--;
        other->read_index++;
        other->read_index %= other->capacity;

        i++;
    }

    return (int32_t)i;
}