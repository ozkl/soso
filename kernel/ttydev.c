#include "alloc.h"
#include "devfs.h"
#include "device.h"
#include "fifobuffer.h"
#include "list.h"
#include "timer.h"
#include "process.h"
#include "errno.h"
#include "ttydev.h"

static BOOL master_open(File *file, uint32_t flags);
static void master_close(File *file);
static int32_t master_read(File *file, uint32_t size, uint8_t *buffer);
static int32_t master_write(File *file, uint32_t size, uint8_t *buffer);
static BOOL master_read_rest_ready(File *file);
static BOOL master_write_test_ready(File *file);

static BOOL slave_open(File *file, uint32_t flags);
static void slave_close(File *file);
static int32_t slave_read(File *file, uint32_t size, uint8_t *buffer);
static int32_t slave_write(File *file, uint32_t size, uint8_t *buffer);
static BOOL slave_read_test_ready(File *file);
static BOOL slave_write_test_ready(File *file);
static int32_t slave_ioctl(File *file, int32_t request, void * argp);

static uint32_t g_name_generator = 0;

static BOOL is_erase_character(TtyDev* tty, uint8_t character)
{
    if (character == tty->term.c_cc[VERASE])
    {
        return TRUE;
    }

    return FALSE;
}

FileSystemNode* ttydev_create()
{
    TtyDev* tty_dev = kmalloc(sizeof(TtyDev));
    memset((uint8_t*)tty_dev, 0, sizeof(TtyDev));

    tty_dev->controlling_process = -1;
    tty_dev->foreground_process = -1;

    tty_dev->winsize.ws_col = 80;
    tty_dev->winsize.ws_row = 25;
    tty_dev->winsize.ws_xpixel = 0;
    tty_dev->winsize.ws_ypixel = 0;

    tty_dev->term.c_cc[VMIN] = 1;
    tty_dev->term.c_cc[VTIME] = 0;
    tty_dev->term.c_cc[VINTR] = 3;
    tty_dev->term.c_cc[VSUSP] = 26;
    tty_dev->term.c_cc[VQUIT] = 28;
    tty_dev->term.c_cc[VERASE] = 127;

    tty_dev->term.c_iflag |= ICRNL;

    tty_dev->term.c_oflag |= ONLCR;

    tty_dev->term.c_lflag |= ECHO;
    tty_dev->term.c_lflag |= ICANON;
    tty_dev->term.c_lflag |= ISIG;

    tty_dev->buffer_master_write = fifobuffer_create(4096);
    tty_dev->buffer_master_read = fifobuffer_create(4096);
    tty_dev->buffer_echo = fifobuffer_create(4096);
    tty_dev->slave_readers = list_create();

    
    spinlock_init(&tty_dev->buffer_master_write_lock);
    spinlock_init(&tty_dev->buffer_master_read_lock);
    spinlock_init(&tty_dev->slave_readers_lock);

    ++g_name_generator;

    Device master;
    memset((uint8_t*)&master, 0, sizeof(Device));
    sprintf(master.name, 16, "ptty%d-m", g_name_generator);
    master.device_type = FT_CHARACTER_DEVICE;
    master.open = master_open;
    master.close = master_close;
    master.read = master_read;
    master.write = master_write;
    master.read_test_ready = master_read_rest_ready;
    master.write_test_ready = master_write_test_ready;
    master.private_data = tty_dev;

    Device slave;
    memset((uint8_t*)&slave, 0, sizeof(Device));
    sprintf(slave.name, 16, "ptty%d", g_name_generator);
    slave.device_type = FT_CHARACTER_DEVICE;
    slave.open = slave_open;
    slave.close = slave_close;
    slave.read = slave_read;
    slave.write = slave_write;
    slave.read_test_ready = slave_read_test_ready;
    slave.write_test_ready = slave_write_test_ready;
    slave.ioctl = slave_ioctl;
    slave.private_data = tty_dev;

    FileSystemNode* master_node = devfs_register_device(&master);
    FileSystemNode* slave_node = devfs_register_device(&slave);

    tty_dev->master_node = master_node;
    tty_dev->slave_node = slave_node;

    return master_node;
}

static void wake_slave_readers(TtyDev* tty)
{
    spinlock_lock(&tty->slave_readers_lock);
    list_foreach(n, tty->slave_readers)
    {
        Thread* thread = (Thread*)n->data;
        if (thread->state == TS_WAITIO && thread->state_privateData == tty)
        {
            thread_resume(thread);
        }
    }
    spinlock_unlock(&tty->slave_readers_lock);
}

static BOOL master_open(File *file, uint32_t flags)
{
    return TRUE;
}

static void master_close(File *file)
{
    
}

static BOOL master_read_rest_ready(File *file)
{
    TtyDev* tty = (TtyDev*)file->node->private_node_data;

    if (spinlock_try_lock(&tty->buffer_master_read_lock))
    {
        uint32_t needed_size = 1;
        uint32_t buffer_len = fifobuffer_get_size(tty->buffer_master_read);
        spinlock_unlock(&tty->buffer_master_read_lock);

        if (buffer_len >= needed_size)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static int32_t master_read(File *file, uint32_t size, uint8_t *buffer)
{
    enable_interrupts();

    if (size > 0)
    {
        TtyDev* tty = (TtyDev*)file->node->private_node_data;

        
        while (TRUE)
        {
            spinlock_lock(&tty->buffer_master_read_lock);

            uint32_t needed_size = 1;
            uint32_t buffer_len = fifobuffer_get_size(tty->buffer_master_read);

            int read_size = -1;

            if (buffer_len >= needed_size)
            {
                read_size = fifobuffer_dequeue(tty->buffer_master_read, buffer, MIN(buffer_len, size));
            }

            spinlock_unlock(&tty->buffer_master_read_lock);

            if (read_size > 0)
            {
                return read_size;
            }

            tty->master_reader = file->thread;
            thread_change_state(file->thread, TS_WAITIO, tty);
            halt();
        }
    }

    return -1;
}

int32_t ttydev_master_read_nonblock(File *file, uint32_t size, uint8_t *buffer)
{
    if (size > 0)
    {
        TtyDev* tty = (TtyDev*)file->node->private_node_data;

        int read_size = 0;
        if (spinlock_try_lock(&tty->buffer_master_read_lock))
        {
            uint32_t needed_size = 1;
            uint32_t buffer_len = fifobuffer_get_size(tty->buffer_master_read);

            if (buffer_len >= needed_size)
            {
                read_size = fifobuffer_dequeue(tty->buffer_master_read, buffer, MIN(buffer_len, size));
            }

            spinlock_unlock(&tty->buffer_master_read_lock);
        }
        return read_size;
    }

    return -1;
}

static BOOL master_write_test_ready(File *file)
{
    return TRUE;
}

static BOOL override_character_master_write(TtyDev* tty, uint8_t* character)
{
    BOOL result = TRUE;

    if (tty->term.c_iflag & ISTRIP)
    {
        //Strip off eighth bit.
		*character &= 0x7F;
	}

    if (*character == '\r' && (tty->term.c_iflag & IGNCR))
    {
        //Ignore carriage return on input.
		result = FALSE;
	}
    else if (*character == '\n' && (tty->term.c_iflag & INLCR))
    {
        //Translate NL to CR on input.
		*character = '\r';
	}
    else if (*character == '\r' && (tty->term.c_iflag & ICRNL))
    {
        //Translate carriage return to newline on input (unless IGNCR is set).
		*character = '\n';
	}

    //The below logic is not in termios but I added for some devices uses \b for backspace
    //(QEMU serial window)
    if (*character == '\b')
    {
        *character = tty->term.c_cc[VERASE];
    }

    return result;
}

static int32_t master_write(File *file, uint32_t size, uint8_t *buffer)
{
    if (size == 0)
    {
        return -1;
    }

    TtyDev* tty = (TtyDev*)file->node->private_node_data;

    fifobuffer_clear(tty->buffer_echo);

    //enableInterrupts(); //TODO: check

    spinlock_lock(&tty->buffer_master_write_lock);

    uint32_t free = fifobuffer_get_free(tty->buffer_master_write);

    uint32_t to_write = MIN(size, free);

    uint32_t written = 0;

    for (uint32_t k = 0; k < to_write; ++k)
    {
        uint8_t character = buffer[k];

        BOOL use_character = override_character_master_write(tty, &character);

        if (use_character == FALSE)
        {
            continue;
        }

        uint8_t escaped[2];
        escaped[0] = 0;
        escaped[1] = 0;

        if ((tty->term.c_lflag & ECHO))
        {
            if (iscntrl(character)
            && !is_erase_character(tty, character)
            && character != '\n'
            && character != '\r')
            {
                escaped[0] = '^';
                escaped[1] = ('@' + character) % 128;
                fifobuffer_enqueue(tty->buffer_echo, escaped, 2);
            }
            else if (character != 127)
            {
                fifobuffer_enqueue(tty->buffer_echo, &character, 1);
            }
        }

        if ((tty->term.c_lflag & ISIG) && tty->foreground_process >= 0)
        {
            if (character == tty->term.c_cc[VINTR])
            {
                process_signal(tty->foreground_process, SIGINT);
            }
            else if (character == tty->term.c_cc[VSUSP])
            {
                process_signal(tty->foreground_process, SIGTSTP);
            }
            else if (character == tty->term.c_cc[VQUIT])
            {
                process_signal(tty->foreground_process, SIGQUIT);
            }
        }

        if (tty->term.c_lflag & ICANON)
        {
            if (tty->line_buffer_index >= TTYDEV_LINEBUFFER_SIZE - 1)
            {
                tty->line_buffer_index = 0;
            }

            if (is_erase_character(tty, character))
            {
                if (tty->line_buffer_index > 0)
                {
                    tty->line_buffer[--tty->line_buffer_index] = '\0';

                    uint8_t c2 = '\b';
                    fifobuffer_enqueue(tty->buffer_echo, &c2, 1);
                    c2 = ' ';
                    fifobuffer_enqueue(tty->buffer_echo, &c2, 1);
                    c2 = '\b';
                    fifobuffer_enqueue(tty->buffer_echo, &c2, 1);
                }
            }
            else if (character == '\n')
            {
                int bytesToCopy = tty->line_buffer_index;

                if (bytesToCopy > 0)
                {
                    tty->line_buffer_index = 0;
                    written = fifobuffer_enqueue(tty->buffer_master_write, tty->line_buffer, bytesToCopy);
                    written += fifobuffer_enqueue(tty->buffer_master_write, (uint8_t*)"\n", 1);

                    if ((tty->term.c_lflag & ECHO))
                    {
                        char c2 = '\r';
                        fifobuffer_enqueue(tty->buffer_echo, (uint8_t*)&c2, 1);
                    }
                }
            }
            else
            {
                if (escaped[0] == '^')
                {
                    if (tty->line_buffer_index < TTYDEV_LINEBUFFER_SIZE - 2)
                    {
                        tty->line_buffer[tty->line_buffer_index++] = escaped[0];
                        tty->line_buffer[tty->line_buffer_index++] = escaped[1];
                    }
                }
                else
                {
                    if (tty->line_buffer_index < TTYDEV_LINEBUFFER_SIZE - 1)
                    {
                        tty->line_buffer[tty->line_buffer_index++] = character;
                    }
                }
            }
        }
        else //non-canonical
        {
            written += fifobuffer_enqueue(tty->buffer_master_write, &character, 1);
        }
    }
    
    spinlock_unlock(&tty->buffer_master_write_lock);

    if (written > 0)
    {
        wake_slave_readers(tty);
    }

    if (fifobuffer_get_size(tty->buffer_echo) > 0)
    {
        spinlock_lock(&tty->buffer_master_read_lock);

        int32_t w = fifobuffer_enqueue_from_other(tty->buffer_master_read, tty->buffer_echo);

        uint32_t bytes_usable = fifobuffer_get_size(tty->buffer_master_read);

        spinlock_unlock(&tty->buffer_master_read_lock);

        if (bytes_usable > 0 && tty->master_read_ready)
        {
            tty->master_read_ready(tty, bytes_usable);
        }

        if (w > 0)
        {
            if (tty->master_reader)
            {
                if (tty->master_reader->state == TS_WAITIO && tty->master_reader->state_privateData == tty)
                {
                    thread_resume(tty->master_reader);
                }
            }
        }
    }


    return (int32_t)written;
}

static BOOL slave_open(File *file, uint32_t flags)
{
    return TRUE;
}

static void slave_close(File *file)
{
    
}

static int32_t slave_ioctl(File *file, int32_t request, void * argp)
{
    TtyDev* tty = (TtyDev*)file->node->private_node_data;


    if (file->node != g_current_thread->owner->tty)
    {
        printkf("-ENOTTY\n");
        return -ENOTTY;
    }

    switch (request)
    {
    case TIOCGPGRP:
        if (!check_user_access(argp))
        {
            return -EFAULT;
        }
        *(int32_t*)argp = tty->foreground_process;
        return 0;
        break;
    case TIOCSPGRP:
        if (!check_user_access(argp))
        {
            return -EFAULT;
        }
        tty->foreground_process = *(int32_t*)argp;
        //char path[80];
        //fs_get_node_path(file->node, path, 80);
        //printkf("setting fg %d of %s by %d\n", tty->foreground_process, path, g_current_thread->owner->pid);
        return 0;
        break;
    case TIOCGWINSZ:
    {
        if (!check_user_access(argp))
        {
            return -EFAULT;
        }
        winsize_t* ws = (winsize_t*)argp;

        *ws = tty->winsize;

        return 0;//success
    }
        break;
    case TIOCSWINSZ:
    {
        if (!check_user_access(argp))
        {
            return -EFAULT;
        }
        winsize_t* ws = (winsize_t*)argp;

        tty->winsize = *ws;

        return 0;//success
    }
        break;
    case TCGETS:
    {
        if (!check_user_access(argp))
        {
            return -EFAULT;
        }
        struct termios* term = (struct termios*)argp;

        *term = tty->term;

        return 0;//success
    }
        break;
    case TCSETS:
    case TCSETSW:
    case TCSETSF:
    {
        if (!check_user_access(argp))
        {
            return -EFAULT;
        }
        struct termios* term = (struct termios*)argp;

        tty->term = *term;

        return 0;//success
    }
        break;
    default:
        printkf("slave_ioctl:Unknown request:%d (%x)\n", request, request);
        break;
    }

    return 0;
}

static BOOL slave_read_test_ready(File *file)
{
    TtyDev* tty = (TtyDev*)file->node->private_node_data;

    if (spinlock_try_lock(&tty->buffer_master_write_lock))
    {
        uint32_t needed_size = 1;
        if ((tty->term.c_lflag & ICANON) != ICANON) //if noncanonical
        {
            needed_size = tty->term.c_cc[VMIN];
        }

        uint32_t buffer_len = fifobuffer_get_size(tty->buffer_master_write);

        spinlock_unlock(&tty->buffer_master_write_lock);

        if (buffer_len >= needed_size)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static int32_t slave_read(File *file, uint32_t size, uint8_t *buffer)
{
    enable_interrupts();

    //TODO: implement VTIME
    //uint32_t timer = get_uptime_milliseconds();

    if (size > 0)
    {
        TtyDev* tty = (TtyDev*)file->node->private_node_data;

        
        while (TRUE)
        {
            spinlock_lock(&tty->buffer_master_write_lock);

            uint32_t needed_size = 1;
            if ((tty->term.c_lflag & ICANON) != ICANON) //if noncanonical
            {
                needed_size = tty->term.c_cc[VMIN];

                if (size < needed_size)
                {
                    needed_size = size;
                }
            }
            uint32_t buffer_len = fifobuffer_get_size(tty->buffer_master_write);

            int read_size = -1;

            if (buffer_len >= needed_size)
            {
                read_size = fifobuffer_dequeue(tty->buffer_master_write, buffer, MIN(buffer_len, size));
            }

            spinlock_unlock(&tty->buffer_master_write_lock);

            if (read_size > 0)
            {
                return read_size;
            }
            else
            {
                if (needed_size == 0)
                {
                    //polling read because MIN==0
                    return 0;
                }
            }

            //TODO: remove reader from list
            spinlock_lock(&tty->slave_readers_lock);
            list_remove_first_occurrence(tty->slave_readers, file->thread);
            list_append(tty->slave_readers, file->thread);
            spinlock_unlock(&tty->slave_readers_lock);

            thread_change_state(file->thread, TS_WAITIO, tty);
            halt();
        }
    }

    return -1;
}

static BOOL slave_write_test_ready(File *file)
{
    return TRUE;
}

static uint32_t process_slave_write(TtyDev* tty, uint32_t size, uint8_t *buffer)
{
    uint32_t written = 0;

    tcflag_t c_oflag = tty->term.c_oflag;

    FifoBuffer* fifo = tty->buffer_master_read;

    uint8_t w = 0;

    for (size_t i = 0; i < size; i++)
    {
        uint8_t c = buffer[i];

        if (c == '\r' && (c_oflag & OCRNL))
        {
            w = '\n';
            if (fifobuffer_enqueue(fifo, &w, 1) > 0)
            {
                written += 1;
            }
        }
        else if (c == '\r' && (c_oflag & ONLRET))
        {
            written += 1;
        }
        else if (c == '\n' && (c_oflag & ONLCR))
        {
            if (fifobuffer_get_free(fifo) >= 2)
            {
                w = '\r';
                fifobuffer_enqueue(fifo, &w, 1);
                w = '\n';
                fifobuffer_enqueue(fifo, &w, 1);

                written += 1;
            }
        }
        else
        {
            if (fifobuffer_enqueue(fifo, &c, 1) > 0)
            {
                written += 1;
            }
        }
    }
    return written;
}

static int32_t slave_write(File *file, uint32_t size, uint8_t *buffer)
{
    if (size == 0)
    {
        return -1;
    }

    TtyDev* tty = (TtyDev*)file->node->private_node_data;

    enable_interrupts();

    spinlock_lock(&tty->buffer_master_read_lock);

    uint32_t free = fifobuffer_get_free(tty->buffer_master_read);

    int written = 0;

    if (free > 0)
    {
        written = process_slave_write(tty, MIN(size, free), buffer);
    }

    uint32_t bytes_usable = fifobuffer_get_size(tty->buffer_master_read);

    spinlock_unlock(&tty->buffer_master_read_lock);

    if (bytes_usable > 0 && tty->master_read_ready)
    {
        tty->master_read_ready(tty, bytes_usable);
    }

    if (written > 0)
    {
        if (tty->master_reader)
        {
            if (tty->master_reader->state == TS_WAITIO && tty->master_reader->state_privateData == tty)
            {
                thread_resume(tty->master_reader);
            }
        }
        
        return written;
    }

    return -1;
}