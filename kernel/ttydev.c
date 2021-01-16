#include "alloc.h"
#include "devfs.h"
#include "device.h"
#include "fifobuffer.h"
#include "list.h"
#include "timer.h"
#include "process.h"
#include "errno.h"
#include "ttydev.h"

static BOOL master_open(File *file, uint32 flags);
static void master_close(File *file);
static int32 master_read(File *file, uint32 size, uint8 *buffer);
static int32 master_write(File *file, uint32 size, uint8 *buffer);
static BOOL master_readTestReady(File *file);
static BOOL master_writeTestReady(File *file);

static BOOL slave_open(File *file, uint32 flags);
static void slave_close(File *file);
static int32 slave_read(File *file, uint32 size, uint8 *buffer);
static int32 slave_write(File *file, uint32 size, uint8 *buffer);
static BOOL slave_readTestReady(File *file);
static BOOL slave_writeTestReady(File *file);
static int32 slave_ioctl(File *file, int32 request, void * argp);

static uint32 gNameGenerator = 0;

static BOOL isEraseCharacter(TtyDev* tty, uint8 character)
{
    if (character == tty->term.c_cc[VERASE])
    {
        return TRUE;
    }

    return FALSE;
}

FileSystemNode* createTTYDev()
{
    TtyDev* ttyDev = kmalloc(sizeof(TtyDev));
    memset((uint8*)ttyDev, 0, sizeof(TtyDev));

    ttyDev->controllingProcess = -1;
    ttyDev->foregroundProcess = -1;

    ttyDev->winsize.ws_col = 80;
    ttyDev->winsize.ws_row = 25;
    ttyDev->winsize.ws_xpixel = 0;
    ttyDev->winsize.ws_ypixel = 0;

    ttyDev->term.c_cc[VMIN] = 1;
    ttyDev->term.c_cc[VTIME] = 0;
    ttyDev->term.c_cc[VINTR] = 3;
    ttyDev->term.c_cc[VSUSP] = 26;
    ttyDev->term.c_cc[VQUIT] = 28;
    ttyDev->term.c_cc[VERASE] = 127;

    ttyDev->term.c_iflag |= ICRNL;

    ttyDev->term.c_oflag |= ONLCR;

    ttyDev->term.c_lflag |= ECHO;
    ttyDev->term.c_lflag |= ICANON;
    ttyDev->term.c_lflag |= ISIG;

    ttyDev->bufferMasterWrite = FifoBuffer_create(4096);
    ttyDev->bufferMasterRead = FifoBuffer_create(4096);
    ttyDev->bufferEcho = FifoBuffer_create(4096);
    ttyDev->slaveReaders = List_Create();

    
    Spinlock_Init(&ttyDev->bufferMasterWriteLock);
    Spinlock_Init(&ttyDev->bufferMasterReadLock);
    Spinlock_Init(&ttyDev->slaveReadersLock);

    ++gNameGenerator;

    Device master;
    memset((uint8*)&master, 0, sizeof(Device));
    sprintf(master.name, "ptty%d-m", gNameGenerator);
    master.device_type = FT_CharacterDevice;
    master.open = master_open;
    master.close = master_close;
    master.read = master_read;
    master.write = master_write;
    master.read_test_ready = master_readTestReady;
    master.write_test_ready = master_writeTestReady;
    master.private_data = ttyDev;

    Device slave;
    memset((uint8*)&slave, 0, sizeof(Device));
    sprintf(slave.name, "ptty%d", gNameGenerator);
    slave.device_type = FT_CharacterDevice;
    slave.open = slave_open;
    slave.close = slave_close;
    slave.read = slave_read;
    slave.write = slave_write;
    slave.read_test_ready = slave_readTestReady;
    slave.write_test_ready = slave_writeTestReady;
    slave.ioctl = slave_ioctl;
    slave.private_data = ttyDev;

    FileSystemNode* masterNode = devfs_register_device(&master);
    FileSystemNode* slaveNode = devfs_register_device(&slave);

    ttyDev->master_node = masterNode;
    ttyDev->slave_node = slaveNode;

    return masterNode;
}

static void wakeSlaveReaders(TtyDev* tty)
{
    Spinlock_Lock(&tty->slaveReadersLock);
    List_Foreach(n, tty->slaveReaders)
    {
        Thread* thread = (Thread*)n->data;
        if (thread->state == TS_WAITIO && thread->state_privateData == tty)
        {
            resumeThread(thread);
        }
    }
    Spinlock_Unlock(&tty->slaveReadersLock);
}

static BOOL master_open(File *file, uint32 flags)
{
    return TRUE;
}

static void master_close(File *file)
{
    
}

static BOOL master_readTestReady(File *file)
{
    TtyDev* tty = (TtyDev*)file->node->privateNodeData;

    if (Spinlock_TryLock(&tty->bufferMasterReadLock))
    {
        uint32 neededSize = 1;
        uint32 bufferLen = FifoBuffer_getSize(tty->bufferMasterRead);
        Spinlock_Unlock(&tty->bufferMasterReadLock);

        if (bufferLen >= neededSize)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static int32 master_read(File *file, uint32 size, uint8 *buffer)
{
    enableInterrupts();

    if (size > 0)
    {
        TtyDev* tty = (TtyDev*)file->node->privateNodeData;

        
        while (TRUE)
        {
            Spinlock_Lock(&tty->bufferMasterReadLock);

            uint32 neededSize = 1;
            uint32 bufferLen = FifoBuffer_getSize(tty->bufferMasterRead);

            int readSize = -1;

            if (bufferLen >= neededSize)
            {
                readSize = FifoBuffer_dequeue(tty->bufferMasterRead, buffer, MIN(bufferLen, size));
            }

            Spinlock_Unlock(&tty->bufferMasterReadLock);

            if (readSize > 0)
            {
                return readSize;
            }

            tty->masterReader = file->thread;
            changeThreadState(file->thread, TS_WAITIO, tty);
            halt();
        }
    }

    return -1;
}

int32 TtyDev_master_read_nonblock(File *file, uint32 size, uint8 *buffer)
{
    if (size > 0)
    {
        TtyDev* tty = (TtyDev*)file->node->privateNodeData;

        int readSize = 0;
        if (Spinlock_TryLock(&tty->bufferMasterReadLock))
        {
            uint32 neededSize = 1;
            uint32 bufferLen = FifoBuffer_getSize(tty->bufferMasterRead);

            if (bufferLen >= neededSize)
            {
                readSize = FifoBuffer_dequeue(tty->bufferMasterRead, buffer, MIN(bufferLen, size));
            }

            Spinlock_Unlock(&tty->bufferMasterReadLock);
        }
        return readSize;
    }

    return -1;
}

static BOOL master_writeTestReady(File *file)
{
    return TRUE;
}

static BOOL overrideCharacterMasterWrite(TtyDev* tty, uint8* character)
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

static int32 master_write(File *file, uint32 size, uint8 *buffer)
{
    if (size == 0)
    {
        return -1;
    }

    TtyDev* tty = (TtyDev*)file->node->privateNodeData;

    FifoBuffer_clear(tty->bufferEcho);

    //enableInterrupts(); //TODO: check

    Spinlock_Lock(&tty->bufferMasterWriteLock);

    uint32 free = FifoBuffer_getFree(tty->bufferMasterWrite);

    uint32 toWrite = MIN(size, free);

    uint32 written = 0;

    for (uint32 k = 0; k < toWrite; ++k)
    {
        uint8 character = buffer[k];

        BOOL useCharacter = overrideCharacterMasterWrite(tty, &character);

        if (useCharacter == FALSE)
        {
            continue;
        }

        uint8 escaped[2];
        escaped[0] = 0;
        escaped[1] = 0;

        if ((tty->term.c_lflag & ECHO))
        {
            if (iscntrl(character)
            && !isEraseCharacter(tty, character)
            && character != '\n'
            && character != '\r')
            {
                escaped[0] = '^';
                escaped[1] = ('@' + character) % 128;
                FifoBuffer_enqueue(tty->bufferEcho, escaped, 2);
            }
            else if (character != 127)
            {
                FifoBuffer_enqueue(tty->bufferEcho, &character, 1);
            }
        }

        if ((tty->term.c_lflag & ISIG) && tty->foregroundProcess >= 0)
        {
            if (character == tty->term.c_cc[VINTR])
            {
                signalProcess(tty->foregroundProcess, SIGINT);
            }
            else if (character == tty->term.c_cc[VSUSP])
            {
                signalProcess(tty->foregroundProcess, SIGTSTP);
            }
            else if (character == tty->term.c_cc[VQUIT])
            {
                signalProcess(tty->foregroundProcess, SIGQUIT);
            }
        }

        if (tty->term.c_lflag & ICANON)
        {
            if (tty->lineBufferIndex >= TTYDEV_LINEBUFFER_SIZE - 1)
            {
                tty->lineBufferIndex = 0;
            }

            if (isEraseCharacter(tty, character))
            {
                if (tty->lineBufferIndex > 0)
                {
                    tty->lineBuffer[--tty->lineBufferIndex] = '\0';

                    uint8 c2 = '\b';
                    FifoBuffer_enqueue(tty->bufferEcho, &c2, 1);
                    c2 = ' ';
                    FifoBuffer_enqueue(tty->bufferEcho, &c2, 1);
                    c2 = '\b';
                    FifoBuffer_enqueue(tty->bufferEcho, &c2, 1);
                }
            }
            else if (character == '\n')
            {
                int bytesToCopy = tty->lineBufferIndex;

                if (bytesToCopy > 0)
                {
                    tty->lineBufferIndex = 0;
                    written = FifoBuffer_enqueue(tty->bufferMasterWrite, tty->lineBuffer, bytesToCopy);
                    written += FifoBuffer_enqueue(tty->bufferMasterWrite, (uint8*)"\n", 1);

                    if ((tty->term.c_lflag & ECHO))
                    {
                        char c2 = '\r';
                        FifoBuffer_enqueue(tty->bufferEcho, (uint8*)&c2, 1);
                    }
                }
            }
            else
            {
                if (escaped[0] == '^')
                {
                    if (tty->lineBufferIndex < TTYDEV_LINEBUFFER_SIZE - 2)
                    {
                        tty->lineBuffer[tty->lineBufferIndex++] = escaped[0];
                        tty->lineBuffer[tty->lineBufferIndex++] = escaped[1];
                    }
                }
                else
                {
                    if (tty->lineBufferIndex < TTYDEV_LINEBUFFER_SIZE - 1)
                    {
                        tty->lineBuffer[tty->lineBufferIndex++] = character;
                    }
                }
            }
        }
        else //non-canonical
        {
            written += FifoBuffer_enqueue(tty->bufferMasterWrite, &character, 1);
        }
    }
    
    Spinlock_Unlock(&tty->bufferMasterWriteLock);

    if (written > 0)
    {
        wakeSlaveReaders(tty);
    }

    if (FifoBuffer_getSize(tty->bufferEcho) > 0)
    {
        Spinlock_Lock(&tty->bufferMasterReadLock);

        int32 w = FifoBuffer_enqueueFromOther(tty->bufferMasterRead, tty->bufferEcho);

        uint32 bytesUsable = FifoBuffer_getSize(tty->bufferMasterRead);

        Spinlock_Unlock(&tty->bufferMasterReadLock);

        if (bytesUsable > 0 && tty->masterReadReady)
        {
            tty->masterReadReady(tty, bytesUsable);
        }

        if (w > 0)
        {
            if (tty->masterReader)
            {
                if (tty->masterReader->state == TS_WAITIO && tty->masterReader->state_privateData == tty)
                {
                    resumeThread(tty->masterReader);
                }
            }
        }
    }


    return (int32)written;
}

static BOOL slave_open(File *file, uint32 flags)
{
    return TRUE;
}

static void slave_close(File *file)
{
    
}

static int32 slave_ioctl(File *file, int32 request, void * argp)
{
    TtyDev* tty = (TtyDev*)file->node->privateNodeData;


    if (file->node != gCurrentThread->owner->tty)
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
        *(int32*)argp = tty->foregroundProcess;
        return 0;
        break;
    case TIOCSPGRP:
        if (!check_user_access(argp))
        {
            return -EFAULT;
        }
        tty->foregroundProcess = *(int32*)argp;
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

static BOOL slave_readTestReady(File *file)
{
    TtyDev* tty = (TtyDev*)file->node->privateNodeData;

    if (Spinlock_TryLock(&tty->bufferMasterWriteLock))
    {
        uint32 neededSize = 1;
        if ((tty->term.c_lflag & ICANON) != ICANON) //if noncanonical
        {
            neededSize = tty->term.c_cc[VMIN];
        }

        uint32 bufferLen = FifoBuffer_getSize(tty->bufferMasterWrite);

        Spinlock_Unlock(&tty->bufferMasterWriteLock);

        if (bufferLen >= neededSize)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static int32 slave_read(File *file, uint32 size, uint8 *buffer)
{
    enableInterrupts();

    //TODO: implement VTIME
    //uint32 timer = get_uptime_milliseconds();

    if (size > 0)
    {
        TtyDev* tty = (TtyDev*)file->node->privateNodeData;

        
        while (TRUE)
        {
            Spinlock_Lock(&tty->bufferMasterWriteLock);

            uint32 neededSize = 1;
            if ((tty->term.c_lflag & ICANON) != ICANON) //if noncanonical
            {
                neededSize = tty->term.c_cc[VMIN];

                if (size < neededSize)
                {
                    neededSize = size;
                }
            }
            uint32 bufferLen = FifoBuffer_getSize(tty->bufferMasterWrite);

            int readSize = -1;

            if (bufferLen >= neededSize)
            {
                readSize = FifoBuffer_dequeue(tty->bufferMasterWrite, buffer, MIN(bufferLen, size));
            }

            Spinlock_Unlock(&tty->bufferMasterWriteLock);

            if (readSize > 0)
            {
                return readSize;
            }
            else
            {
                if (neededSize == 0)
                {
                    //polling read because MIN==0
                    return 0;
                }
            }

            //TODO: remove reader from list
            Spinlock_Lock(&tty->slaveReadersLock);
            List_RemoveFirstOccurrence(tty->slaveReaders, file->thread);
            List_Append(tty->slaveReaders, file->thread);
            Spinlock_Unlock(&tty->slaveReadersLock);

            changeThreadState(file->thread, TS_WAITIO, tty);
            halt();
        }
    }

    return -1;
}

static BOOL slave_writeTestReady(File *file)
{
    return TRUE;
}

static uint32 processSlaveWrite(TtyDev* tty, uint32 size, uint8 *buffer)
{
    uint32 written = 0;

    tcflag_t c_oflag = tty->term.c_oflag;

    FifoBuffer* fifo = tty->bufferMasterRead;

    uint8 w = 0;

    for (size_t i = 0; i < size; i++)
    {
        uint8 c = buffer[i];

        if (c == '\r' && (c_oflag & OCRNL))
        {
            w = '\n';
            if (FifoBuffer_enqueue(fifo, &w, 1) > 0)
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
            if (FifoBuffer_getFree(fifo) >= 2)
            {
                w = '\r';
                FifoBuffer_enqueue(fifo, &w, 1);
                w = '\n';
                FifoBuffer_enqueue(fifo, &w, 1);

                written += 1;
            }
        }
        else
        {
            if (FifoBuffer_enqueue(fifo, &c, 1) > 0)
            {
                written += 1;
            }
        }
    }
    return written;
}

static int32 slave_write(File *file, uint32 size, uint8 *buffer)
{
    if (size == 0)
    {
        return -1;
    }

    TtyDev* tty = (TtyDev*)file->node->privateNodeData;

    enableInterrupts();

    Spinlock_Lock(&tty->bufferMasterReadLock);

    uint32 free = FifoBuffer_getFree(tty->bufferMasterRead);

    int written = 0;

    if (free > 0)
    {
        written = processSlaveWrite(tty, MIN(size, free), buffer);
    }

    uint32 bytesUsable = FifoBuffer_getSize(tty->bufferMasterRead);

    Spinlock_Unlock(&tty->bufferMasterReadLock);

    if (bytesUsable > 0 && tty->masterReadReady)
    {
        tty->masterReadReady(tty, bytesUsable);
    }

    if (written > 0)
    {
        if (tty->masterReader)
        {
            if (tty->masterReader->state == TS_WAITIO && tty->masterReader->state_privateData == tty)
            {
                resumeThread(tty->masterReader);
            }
        }
        
        return written;
    }

    return -1;
}