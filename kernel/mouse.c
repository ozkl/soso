#include "mouse.h"
#include "isr.h"
#include "common.h"
#include "device.h"
#include "alloc.h"
#include "devfs.h"
#include "list.h"
#include "fifobuffer.h"
#include "spinlock.h"

static uint8 g_mouse_byte_counter = 0;

static void prepare_for_read();
static void prepare_for_write();
static void write_mouse(uint8 data);
static void handle_mouse_interrupt(Registers *regs);

static BOOL mouse_open(File *file, uint32 flags);
static void mouse_close(File *file);
static int32 mouse_read(File *file, uint32 size, uint8 *buffer);

#define MOUSE_PACKET_SIZE 3

uint8 g_mouse_packet[MOUSE_PACKET_SIZE];

static List* g_readers = NULL;

static Spinlock g_readers_lock;

void initialize_mouse()
{
    Device device;
    memset((uint8*)&device, 0, sizeof(Device));
    strcpy(device.name, "psaux");
    device.device_type = FT_CHARACTER_DEVICE;
    device.open = mouse_open;
    device.close = mouse_close;
    device.read = mouse_read;
    interrupt_register(IRQ12, handle_mouse_interrupt);

    devfs_register_device(&device);

    memset(g_mouse_packet, 0, MOUSE_PACKET_SIZE);

    g_readers = list_create();

    Spinlock_Init(&g_readers_lock);

    prepare_for_write();

    outb(0x64, 0x20); //get status command

    uint8 status = inb(0x60);
    status = status | 2; //enable IRQ12

    outb(0x64, 0x60); //set status command
    outb(0x60, status);

    outb(0x64, 0xA8); //enable Auxiliary Device command

    write_mouse(0xF4); //0xF4: Enable Packet Streaming
}

static BOOL mouse_open(File *file, uint32 flags)
{
    FifoBuffer* fifo = fifobuffer_create(60);

    file->private_data = (void*)fifo;

    Spinlock_Lock(&g_readers_lock);

    list_append(g_readers, file);

    Spinlock_Unlock(&g_readers_lock);

    return TRUE;
}

static void mouse_close(File *file)
{
    Spinlock_Lock(&g_readers_lock);

    list_remove_first_occurrence(g_readers, file);

    Spinlock_Unlock(&g_readers_lock);

    FifoBuffer* fifo = (FifoBuffer*)file->private_data;

    fifobuffer_destroy(fifo);
}

static int32 mouse_read(File *file, uint32 size, uint8 *buffer)
{
    FifoBuffer* fifo = (FifoBuffer*)file->private_data;

    while (fifobuffer_get_size(fifo) < MOUSE_PACKET_SIZE)
    {
        thread_change_state(file->thread, TS_WAITIO, mouse_read);

        enable_interrupts();
        halt();
    }

    disable_interrupts();


    uint32 available = fifobuffer_get_size(fifo);
    uint32 smaller = MIN(available, size);

    fifobuffer_dequeue(fifo, buffer, smaller);

    return smaller;
}

static void prepare_for_read()
{
    //https://wiki.osdev.org/Mouse_Input
    //Bytes cannot be read from port 0x60 until bit 0 (value=1) of port 0x64 is set

    int32 try_count = 1000;

    uint8 data = 0;
    do
    {
        data = inb(0x64);
    } while (((data & 0x01) == 0) && --try_count > 0);
}

static void prepare_for_write()
{
    //https://wiki.osdev.org/Mouse_Input
    //All output to port 0x60 or 0x64 must be preceded by waiting for bit 1 (value=2) of port 0x64 to become clear

    int32 try_count = 1000;

    uint8 data = 0;
    do
    {
        data = inb(0x64);
    } while (((data & 0x02) != 0) && --try_count > 0);
}

static void write_mouse(uint8 data)
{
    prepare_for_write();

    outb(0x64, 0xD4);

    prepare_for_write();

    outb(0x60, data);
}

static void handle_mouse_interrupt(Registers *regs)
{
    uint8 status = 0;
    //0x20 (5th bit is mouse bit)
    //read from 0x64, if its mouse bit is 1 then data is available at 0x60!

    int32 try_count = 1000;
    do
    {
        status = inb(0x64);
    } while (((status & 0x20) == 0) && --try_count > 0);

    uint8 data = inb(0x60);

    g_mouse_packet[g_mouse_byte_counter] = data;

    g_mouse_byte_counter += 1;

    if (g_mouse_byte_counter == MOUSE_PACKET_SIZE)
    {
        g_mouse_byte_counter = 0;

        Spinlock_Lock(&g_readers_lock);

        //Wake readers
        list_foreach(n, g_readers)
        {
            File* file = n->data;

            FifoBuffer* fifo = (FifoBuffer*)file->private_data;

            fifobuffer_enqueue(fifo, g_mouse_packet, MOUSE_PACKET_SIZE);

            if (file->thread->state == TS_WAITIO)
            {
                if (file->thread->state_privateData == mouse_read)
                {
                    thread_resume(file->thread);
                }
            }
        }

        Spinlock_Unlock(&g_readers_lock);
    }

    //printkf("mouse:%d\n", data);
}
