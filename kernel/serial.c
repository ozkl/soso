#include "common.h"
#include "device.h"
#include "devfs.h"
#include "isr.h"
#include "fifobuffer.h"
#include "process.h"
#include "list.h"
#include "serial.h"

#define PORT 0x3f8   //COM1

static FifoBuffer* g_buffer_com1 = NULL;
static List* g_accessing_threads = NULL;

static void handle_serial_interrupt(Registers *regs);

static BOOL serial_open(File *file, uint32_t flags);
static void serial_close(File *file);
static BOOL serial_read_test_ready(File *file);
static int32_t serial_read(File *file, uint32_t size, uint8_t *buffer);
static BOOL serial_write_test_ready(File *file);
static int32_t serial_write(File *file, uint32_t size, uint8_t *buffer);

//TODO: support more than one serial devices

void serial_initialize()
{
    outb(PORT + 1, 0x00);    // Disable all interrupts
    outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(PORT + 1, 0x00);    //                  (hi byte)
    outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    outb(PORT + 1, 0x01);    // Enable interrupts

    interrupt_register(IRQ4, handle_serial_interrupt);

    g_buffer_com1 = fifobuffer_create(4096);
    g_accessing_threads = list_create();

    Device device;
    memset((uint8_t*)&device, 0, sizeof(Device));
    strcpy(device.name, "com1");
    device.device_type = FT_CHARACTER_DEVICE;
    device.open = serial_open;
    device.close = serial_close;
    device.read = serial_read;
    device.write = serial_write;
    device.read_test_ready = serial_read_test_ready;
    device.write_test_ready = serial_write_test_ready;

    devfs_register_device(&device);
}

static int port_received()
{
   return inb(PORT + 5) & 1;
}

static char port_read()
{
   while (port_received() == 0);

   return inb(PORT);
}

static int port_is_transmit_empty()
{
   return inb(PORT + 5) & 0x20;
}

static void port_write(char a)
{
   while (port_is_transmit_empty() == 0);

   outb(PORT,a);
}

static void port_write_buffer(const char *buffer, int n)
{
    for (int i = 0; i < n; ++i)
    {
        port_write(buffer[i]);
    }
}

static void handle_serial_interrupt(Registers *regs)
{
    uint8_t c = (uint8_t)port_read();

    //if buffer is full, we miss the data
    fifobuffer_enqueue(g_buffer_com1, &c, 1);

    list_foreach (n, g_accessing_threads)
    {
        Thread* reader = n->data;

        if (reader->state == TS_WAITIO)
        {
            if (reader->state_privateData == serial_read)
            {
                thread_resume(reader);
            }
        }
    }
}

void serial_printf(const char *format, ...)
{
  char **arg = (char **) &format;
  char c;
  char buf[20];

  //arg++;
  __builtin_va_list vl;
  __builtin_va_start(vl, format);

  while ((c = *format++) != 0)
    {
      if (c != '%')
        port_write(c);
      else
        {
          char *p;

          c = *format++;
          switch (c)
            {
            case 'x':
               buf[0] = '0';
               buf[1] = 'x';
               //itoa (buf + 2, c, *((int *) arg++));
               itoa (buf + 2, c, __builtin_va_arg(vl, int));
               p = buf;
               goto string;
               break;
            case 'd':
            case 'u':
              //itoa (buf, c, *((int *) arg++));
              itoa (buf, c, __builtin_va_arg(vl, int));
              p = buf;
              goto string;
              break;

            case 's':
              //p = *arg++;
              p = __builtin_va_arg(vl, char*);
              if (! p)
                p = "(null)";

            string:
              while (*p)
                port_write(*p++);
              break;

            default:
              //port_write(*((int *) arg++));
              port_write(__builtin_va_arg(vl, int));
              break;
            }
        }
    }
  __builtin_va_end(vl);
}

static BOOL serial_open(File *file, uint32_t flags)
{
    list_append(g_accessing_threads, file->thread);

    return TRUE;
}

static void serial_close(File *file)
{
    list_remove_first_occurrence(g_accessing_threads, file->thread);
}

static BOOL serial_read_test_ready(File *file)
{
    if (fifobuffer_get_size(g_buffer_com1) > 0)
    {
        return TRUE;
    }

    return FALSE;
}

static int32_t serial_read(File *file, uint32_t size, uint8_t *buffer)
{
    if (0 == size || NULL == buffer)
    {
        return -1;
    }

    while (serial_read_test_ready(file) == FALSE)
    {
        thread_change_state(file->thread, TS_WAITIO, serial_read);
        enable_interrupts();
        halt();
    }

    disable_interrupts();

    thread_resume(file->thread);

    int32_t read_bytes = fifobuffer_dequeue(g_buffer_com1, buffer, size);

    return read_bytes;
}

static BOOL serial_write_test_ready(File *file)
{
    return TRUE;
}

static int32_t serial_write(File *file, uint32_t size, uint8_t *buffer)
{
    port_write_buffer((char*)buffer, (int)size);
    
    return size;
}