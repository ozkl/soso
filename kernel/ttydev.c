#include "alloc.h"
#include "fs.h"
#include "devfs.h"
#include "device.h"
#include "ttydev.h"

static BOOL master_open(File *file, uint32 flags);
static void master_close(File *file);
static int32 master_read(File *file, uint32 size, uint8 *buffer);
static int32 master_write(File *file, uint32 size, uint8 *buffer);

static BOOL slave_open(File *file, uint32 flags);
static void slave_close(File *file);
static int32 slave_read(File *file, uint32 size, uint8 *buffer);
static int32 slave_write(File *file, uint32 size, uint8 *buffer);

static uint32 gNameGenerator = 0;

FileSystemNode* createTTYDev()
{
    TtyDev* ttyDev = kmalloc(sizeof(TtyDev));
    memset((uint8*)ttyDev, 0, sizeof(TtyDev));

    ttyDev->term.c_cc[VMIN] = 1;
    ttyDev->term.c_lflag |= ECHO;
    ttyDev->term.c_lflag |= ICANON;

    ++gNameGenerator;

    Device master;
    memset((uint8*)&master, 0, sizeof(Device));
    sprintf(master.name, "ptty%d-m", gNameGenerator);
    master.deviceType = FT_CharacterDevice;
    master.open = master_open;
    master.close = master_close;
    master.read = master_read;
    master.write = master_write;
    master.privateData = ttyDev;

    Device slave;
    memset((uint8*)&slave, 0, sizeof(Device));
    sprintf(slave.name, "ptty%d", gNameGenerator);
    slave.deviceType = FT_CharacterDevice;
    slave.open = slave_open;
    slave.close = slave_close;
    slave.read = slave_read;
    slave.write = slave_write;
    slave.privateData = ttyDev;

    FileSystemNode* masterNode = registerDevice(&master);
    FileSystemNode* slaveNode = registerDevice(&slave);

    ttyDev->masterNode = masterNode;
    ttyDev->slaveNode = slaveNode;

    return masterNode;
}

static BOOL master_open(File *file, uint32 flags)
{
    return TRUE;
}

static void master_close(File *file)
{
    
}

static int32 master_read(File *file, uint32 size, uint8 *buffer)
{
    return -1;
}

static int32 master_write(File *file, uint32 size, uint8 *buffer)
{
    return -1;
}

static BOOL slave_open(File *file, uint32 flags)
{
    return TRUE;
}

static void slave_close(File *file)
{
    
}

static int32 slave_read(File *file, uint32 size, uint8 *buffer)
{
    return -1;
}

static int32 slave_write(File *file, uint32 size, uint8 *buffer)
{
    return -1;
}