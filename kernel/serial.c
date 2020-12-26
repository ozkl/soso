#include "common.h"
#include "device.h"
#include "devfs.h"
#include "isr.h"
#include "fifobuffer.h"
#include "process.h"
#include "list.h"
#include "serial.h"

#define PORT 0x3f8   //COM1

static FifoBuffer* gBufferCom1 = NULL;
static List* gAccessingThreads = NULL;

static void handleSerialInterrupt(Registers *regs);

static BOOL serial_open(File *file, uint32 flags);
static void serial_close(File *file);
static BOOL serial_readTestReady(File *file);
static int32 serial_read(File *file, uint32 size, uint8 *buffer);
static BOOL serial_writeTestReady(File *file);
static int32 serial_write(File *file, uint32 size, uint8 *buffer);

//TODO: support more than one serial devices

void initializeSerial()
{
    outb(PORT + 1, 0x00);    // Disable all interrupts
    outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(PORT + 1, 0x00);    //                  (hi byte)
    outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    outb(PORT + 1, 0x01);    // Enable interrupts

    registerInterruptHandler(IRQ4, handleSerialInterrupt);

    gBufferCom1 = FifoBuffer_create(4096);
    gAccessingThreads = List_Create();

    Device device;
    memset((uint8*)&device, 0, sizeof(Device));
    strcpy(device.name, "com1");
    device.deviceType = FT_CharacterDevice;
    device.open = serial_open;
    device.close = serial_close;
    device.read = serial_read;
    device.write = serial_write;
    device.readTestReady = serial_readTestReady;
    device.writeTestReady = serial_writeTestReady;

    registerDevice(&device);
}

int serialReceived()
{
   return inb(PORT + 5) & 1;
}

char readSerial()
{
   while (serialReceived() == 0);

   return inb(PORT);
}

int isTransmitEmpty()
{
   return inb(PORT + 5) & 0x20;
}

void writeSerial(char a)
{
   while (isTransmitEmpty() == 0);

   outb(PORT,a);
}

void Serial_Write(const char *buffer, int n)
{
    for (int i = 0; i < n; ++i)
    {
        writeSerial(buffer[i]);
    }
}

static void handleSerialInterrupt(Registers *regs)
{
    uint8 c = (uint8)readSerial();

    //if buffer is full, we miss the data
    FifoBuffer_enqueue(gBufferCom1, &c, 1);

    List_Foreach (n, gAccessingThreads)
    {
        Thread* reader = n->data;

        if (reader->state == TS_WAITIO)
        {
            if (reader->state_privateData == serial_read)
            {
                resumeThread(reader);
            }
        }
    }
}

void Serial_PrintF(const char *format, ...)
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
        writeSerial(c);
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
                writeSerial(*p++);
              break;

            default:
              //writeSerial(*((int *) arg++));
              writeSerial(__builtin_va_arg(vl, int));
              break;
            }
        }
    }
  __builtin_va_end(vl);
}

static BOOL serial_open(File *file, uint32 flags)
{
    List_Append(gAccessingThreads, file->thread);

    return TRUE;
}

static void serial_close(File *file)
{
    List_RemoveFirstOccurrence(gAccessingThreads, file->thread);
}

static BOOL serial_readTestReady(File *file)
{
    if (FifoBuffer_getSize(gBufferCom1) > 0)
    {
        return TRUE;
    }

    return FALSE;
}

static int32 serial_read(File *file, uint32 size, uint8 *buffer)
{
    if (0 == size || NULL == buffer)
    {
        return -1;
    }

    while (serial_readTestReady(file) == FALSE)
    {
        changeThreadState(file->thread, TS_WAITIO, serial_read);
        enableInterrupts();
        halt();
    }

    disableInterrupts();

    resumeThread(file->thread);

    int32 readBytes = FifoBuffer_dequeue(gBufferCom1, buffer, size);

    return readBytes;
}

static BOOL serial_writeTestReady(File *file)
{
    return TRUE;
}

static int32 serial_write(File *file, uint32 size, uint8 *buffer)
{
    Serial_Write((char*)buffer, (int)size);
    
    return size;
}