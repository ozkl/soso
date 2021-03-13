#include "keyboard.h"
#include "isr.h"
#include "common.h"
#include "fs.h"
#include "device.h"
#include "alloc.h"
#include "devfs.h"
#include "list.h"
#include "console.h"

static uint8_t* g_key_buffer = NULL;
static uint32_t g_key_buffer_write_index = 0;
static uint32_t g_key_buffer_read_index = 0;

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
    uint32_t read_index;
    ReadMode read_mode;
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

    devfs_register_device(&device);

    interrupt_register(IRQ1, handle_keyboard_interrupt);
}

# define O_NONBLOCK	  04000

static BOOL keyboard_open(File *file, uint32_t flags)
{
    Reader* reader = (Reader*)kmalloc(sizeof(Reader));
    reader->read_index = 0;
    reader->read_mode = RM_BLOCKING;

    if (flags & O_NONBLOCK)
    {
        reader->read_mode = RM_NON_BLOCKING;
        //printkf("keyboard opened as non blocking by %d\n", g_current_thread->owner->pid);
    }

    if (g_key_buffer_write_index > 0)
    {
        reader->read_index = g_key_buffer_write_index;
    }
    file->private_data = (void*)reader;

    list_append(g_readers, file);

    return TRUE;
}

static void keyboard_close(File *file)
{
    Reader* reader = (Reader*)file->private_data;

    kfree(reader);

    list_remove_first_occurrence(g_readers, file);
}

static int32_t keyboard_read(File *file, uint32_t size, uint8_t *buffer)
{
    Reader* reader = (Reader*)file->private_data;

    uint32_t read_index = reader->read_index;

    if (reader->read_mode == RM_BLOCKING)
    {
        while (read_index == g_key_buffer_write_index)
        {
            thread_change_state(file->thread, TS_WAITIO, keyboard_read);
            
            enable_interrupts();
            halt();
        }
    }

    disable_interrupts();

    if (read_index == g_key_buffer_write_index)
    {
        //non-blocking return here
        return 0;
    }

    buffer[0] = g_key_buffer[read_index];
    read_index++;
    read_index %= KEYBUFFER_SIZE;

    reader->read_index = read_index;

    return 1;
}

static BOOL keyboard_read_test_ready(File *file)
{
    Reader* reader = (Reader*)file->private_data;

    uint32_t read_index = reader->read_index;

    if (read_index == g_key_buffer_write_index)
    {
        return FALSE;
    }

    return TRUE;
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

    g_key_buffer[g_key_buffer_write_index] = scancode;
    g_key_buffer_write_index++;
    g_key_buffer_write_index %= KEYBUFFER_SIZE;

    //Wake readers
    list_foreach(n, g_readers)
    {
        File* file = n->data;

        if (file->thread->state == TS_WAITIO)
        {
            if (file->thread->state_privateData == keyboard_read)
            {
                thread_resume(file->thread);
            }
        }
    }

    console_send_key(scancode);
}
