/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#include "mouse.h"
#include "isr.h"
#include "common.h"
#include "device.h"
#include "alloc.h"
#include "devfs.h"
#include "list.h"
#include "fifobuffer.h"
#include "spinlock.h"
#include "errno.h"
#include "waitqueue.h"

static uint8_t g_mouse_byte_counter = 0;

static void prepare_for_read();
static void prepare_for_write();
static void write_mouse(uint8_t data);
static void handle_mouse_interrupt(Registers *regs);

static BOOL mouse_open(File *file, uint32_t flags);
static void mouse_close(File *file);
static BOOL mouse_read_test_ready(File *file);
static int32_t mouse_read(File *file, uint32_t size, uint8_t *buffer);

#define MOUSE_PACKET_SIZE 3

uint8_t g_mouse_packet[MOUSE_PACKET_SIZE];

static List* g_readers = NULL;

static Spinlock g_readers_lock;

typedef struct MouseReader
{
    FifoBuffer* fifo;
    WaitQueue waiters;
} MouseReader;

void initialize_mouse()
{
    Device device;
    memset((uint8_t*)&device, 0, sizeof(Device));
    strcpy(device.name, "psaux");
    device.device_type = FT_CHARACTER_DEVICE;
    device.open = mouse_open;
    device.close = mouse_close;
    device.read_test_ready = mouse_read_test_ready;
    device.read = mouse_read;
    interrupt_register(IRQ12, handle_mouse_interrupt);

    devfs_register_device(&device, TRUE);

    memset(g_mouse_packet, 0, MOUSE_PACKET_SIZE);

    g_readers = list_create();

    spinlock_init(&g_readers_lock);

    prepare_for_write();

    outb(0x64, 0x20); //get status command

    uint8_t status = inb(0x60);
    status = status | 2; //enable IRQ12

    outb(0x64, 0x60); //set status command
    outb(0x60, status);

    outb(0x64, 0xA8); //enable Auxiliary Device command

    write_mouse(0xF4); //0xF4: Enable Packet Streaming
}

static BOOL mouse_open(File *file, uint32_t flags)
{
    MouseReader* reader = kmalloc(sizeof(MouseReader));
    memset((uint8_t*)reader, 0, sizeof(MouseReader));
    reader->fifo = fifobuffer_create(60);
    waitqueue_initialize(&reader->waiters);

    file->private_data = reader;

    begin_critical_section();
    spinlock_lock(&g_readers_lock);

    list_append(g_readers, file);

    spinlock_unlock(&g_readers_lock);
    end_critical_section();

    return TRUE;
}

static void mouse_close(File *file)
{
    begin_critical_section();
    spinlock_lock(&g_readers_lock);

    list_remove_first_occurrence(g_readers, file);

    spinlock_unlock(&g_readers_lock);

    MouseReader* reader = (MouseReader*)file->private_data;

    waitqueue_destroy(&reader->waiters);
    fifobuffer_destroy(reader->fifo);
    kfree(reader);
    file->private_data = NULL;

    end_critical_section();
}

static BOOL mouse_read_test_ready(File *file)
{
    MouseReader* reader = (MouseReader*)file->private_data;

    begin_critical_section();
    BOOL ready = fifobuffer_get_size(reader->fifo) > 0;
    end_critical_section();

    return ready;
}

static int32_t mouse_read(File *file, uint32_t size, uint8_t *buffer)
{
    if (size == 0 || buffer == NULL)
    {
        return -1;
    }

    MouseReader* reader = (MouseReader*)file->private_data;
    FifoBuffer* fifo = reader->fifo;

    disable_interrupts();

    if (file->status_flags & O_NONBLOCK)
    {
        if (fifobuffer_get_size(fifo) == 0)
        {
            return -EAGAIN;
        }
    }
    else
    {
        while (fifobuffer_get_size(fifo) == 0)
        {
            if (g_current_thread->pending_signal_count > 0)
            {
                return -EINTR;
            }

            waitqueue_wait(&reader->waiters, mouse_read);
        }
    }

    uint32_t available = fifobuffer_get_size(fifo);
    uint32_t smaller = MIN(available, size);

    fifobuffer_dequeue(fifo, buffer, smaller);

    return smaller;
}

static void prepare_for_read()
{
    //https://wiki.osdev.org/Mouse_Input
    //Bytes cannot be read from port 0x60 until bit 0 (value=1) of port 0x64 is set

    int32_t try_count = 1000;

    uint8_t data = 0;
    do
    {
        data = inb(0x64);
    } while (((data & 0x01) == 0) && --try_count > 0);
}

static void prepare_for_write()
{
    //https://wiki.osdev.org/Mouse_Input
    //All output to port 0x60 or 0x64 must be preceded by waiting for bit 1 (value=2) of port 0x64 to become clear

    int32_t try_count = 1000;

    uint8_t data = 0;
    do
    {
        data = inb(0x64);
    } while (((data & 0x02) != 0) && --try_count > 0);
}

static void write_mouse(uint8_t data)
{
    prepare_for_write();

    outb(0x64, 0xD4);

    prepare_for_write();

    outb(0x60, data);
}

static void handle_mouse_interrupt(Registers *regs)
{
    uint8_t status = 0;
    //0x20 (5th bit is mouse bit)
    //read from 0x64, if its mouse bit is 1 then data is available at 0x60!

    int32_t try_count = 1000;
    do
    {
        status = inb(0x64);
    } while (((status & 0x20) == 0) && --try_count > 0);

    uint8_t data = inb(0x60);

    if (g_mouse_byte_counter == 0 && !(data & 8))
    {
        return;
    }

    g_mouse_packet[g_mouse_byte_counter] = data;

    g_mouse_byte_counter += 1;

    if (g_mouse_byte_counter == MOUSE_PACKET_SIZE)
    {
        g_mouse_byte_counter = 0;

        spinlock_lock(&g_readers_lock);

        //Wake readers
        list_foreach(n, g_readers)
        {
            File* file = n->data;

            MouseReader* reader = (MouseReader*)file->private_data;
            FifoBuffer* fifo = reader->fifo;

            if (fifobuffer_get_free(fifo) >= MOUSE_PACKET_SIZE)
            {
                fifobuffer_enqueue(fifo, g_mouse_packet, MOUSE_PACKET_SIZE);
                waitqueue_wake_all(&reader->waiters, mouse_read);
            }
        }

        spinlock_unlock(&g_readers_lock);
    }

    //printkf("mouse:%d\n", data);
}
