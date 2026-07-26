/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#include "keyboard.h"
#include "isr.h"
#include "common.h"
#include "fs.h"
#include "device.h"
#include "alloc.h"
#include "devfs.h"
#include "list.h"
#include "console.h"
#include "waitqueue.h"
#include "errno.h"

static uint8_t* g_key_buffer = NULL;
static uint64_t g_key_buffer_write_sequence = 0;

#define KEYBUFFER_SIZE 128

static BOOL keyboard_open(File *file, uint32_t flags);
static void keyboard_close(File *file);
static int32_t keyboard_read(File *file, uint32_t size, uint8_t *buffer);
static BOOL keyboard_read_test_ready(File *file);
static int32_t keyboard_ioctl(File *file, int32_t request, void * argp);

typedef enum ReadMode
{
    RM_BLOCKING = 0,
    RM_NON_BLOCKING = 1
} ReadMode;

typedef struct Reader
{
    uint64_t read_sequence;
    ReadMode read_mode;
    WaitQueue waiters;
} Reader;

static List* g_readers = NULL;

static void handle_keyboard_interrupt(Registers *regs);

void keyboard_initialize()
{
    Device device;
    memset((uint8_t*)&device, 0, sizeof(Device));
    strcpy(device.name, "keyboard");
    device.device_type = FT_CHARACTER_DEVICE;
    device.open = keyboard_open;
    device.close = keyboard_close;
    device.read = keyboard_read;
    device.read_test_ready = keyboard_read_test_ready;
    device.ioctl = keyboard_ioctl;

    g_key_buffer = kmalloc(KEYBUFFER_SIZE);
    memset((uint8_t*)g_key_buffer, 0, KEYBUFFER_SIZE);

    g_readers = list_create();

    devfs_register_device(&device, TRUE);

    interrupt_register(IRQ1, handle_keyboard_interrupt);
}

static BOOL keyboard_open(File *file, uint32_t flags)
{
    Reader* reader = (Reader*)kmalloc(sizeof(Reader));
    memset((uint8_t*)reader, 0, sizeof(Reader));
    reader->read_mode = RM_BLOCKING;
    waitqueue_initialize(&reader->waiters);

    begin_critical_section();

    reader->read_sequence = g_key_buffer_write_sequence;
    file->private_data = (void*)reader;

    list_append(g_readers, file);

    end_critical_section();

    return TRUE;
}

static void keyboard_close(File *file)
{
    Reader* reader = (Reader*)file->private_data;

    begin_critical_section();

    list_remove_first_occurrence(g_readers, file);
    waitqueue_destroy(&reader->waiters);
    kfree(reader);
    file->private_data = NULL;

    end_critical_section();
}

static int32_t keyboard_read(File *file, uint32_t size, uint8_t *buffer)
{
    if (size == 0 || buffer == NULL)
    {
        return -1;
    }

    Reader* reader = (Reader*)file->private_data;

    if (reader->read_mode == RM_BLOCKING &&
        !(file->status_flags & O_NONBLOCK))
    {
        while (TRUE)
        {
            disable_interrupts();

            if (reader->read_sequence != g_key_buffer_write_sequence)
            {
                break;
            }

            if (g_current_thread->pending_signal_count > 0)
            {
                return -EINTR;
            }

            waitqueue_wait(&reader->waiters, keyboard_read);
        }
    }
    else
    {
        disable_interrupts();
    }

    if (reader->read_sequence == g_key_buffer_write_sequence)
    {
        return -EAGAIN;
    }

    uint64_t oldest_sequence = 0;
    if (g_key_buffer_write_sequence > KEYBUFFER_SIZE)
    {
        oldest_sequence = g_key_buffer_write_sequence - KEYBUFFER_SIZE;
    }

    if (reader->read_sequence < oldest_sequence)
    {
        reader->read_sequence = oldest_sequence;
    }

    uint32_t read_index = (uint32_t)(reader->read_sequence & (KEYBUFFER_SIZE - 1));
    buffer[0] = g_key_buffer[read_index];
    ++reader->read_sequence;

    return 1;
}

static BOOL keyboard_read_test_ready(File *file)
{
    Reader* reader = (Reader*)file->private_data;

    begin_critical_section();
    BOOL ready = reader->read_sequence != g_key_buffer_write_sequence;
    end_critical_section();

    return ready;
}

static int32_t keyboard_ioctl(File *file, int32_t request, void * argp)
{
    Reader* reader = (Reader*)file->private_data;

    int cmd = (int)argp;

    switch (request)
    {
    case 0: //get
        *(int*)argp = (int)reader->read_mode;
        return 0;
        break;
    case 1: //set
        if (cmd == 0)
        {
            reader->read_mode = RM_BLOCKING;

            return 0;
        }
        else if (cmd == 1)
        {
            reader->read_mode = RM_NON_BLOCKING;
            //printkf("keyboard set as non blocking by %d\n", g_current_thread->owner->pid);
            return 0;
        }
        break;
    default:
        break;
    }

    return -1;
}

static void handle_keyboard_interrupt(Registers *regs)
{
    uint8_t scancode = 0;
    do
    {
        scancode = inb(0x64);
    } while ((scancode & 0x01) == 0);

    scancode = inb(0x60);

    uint32_t write_index = (uint32_t)(g_key_buffer_write_sequence & (KEYBUFFER_SIZE - 1));
    g_key_buffer[write_index] = scancode;
    ++g_key_buffer_write_sequence;

    //Wake readers
    list_foreach(n, g_readers)
    {
        File* file = n->data;
        Reader* reader = (Reader*)file->private_data;

        waitqueue_wake_all(&reader->waiters, keyboard_read);
    }

    console_send_key(scancode);
}
