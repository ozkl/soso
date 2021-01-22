#include "ramdisk.h"
#include "alloc.h"
#include "fs.h"
#include "devfs.h"

typedef struct Ramdisk
{
    uint8* buffer;
    uint32 size;
} Ramdisk;

#define RAMDISK_BLOCKSIZE 512

static BOOL open(File *file, uint32 flags);
static void close(File *file);
static int32 read_block(FileSystemNode* node, uint32 block_number, uint32 count, uint8* buffer);
static int32 write_block(FileSystemNode* node, uint32 block_number, uint32 count, uint8* buffer);
static int32 ioctl(File *node, int32 request, void * argp);

BOOL ramdisk_create(const char* devName, uint32 size)
{
    Ramdisk* ramdisk = kmalloc(sizeof(Ramdisk));
    ramdisk->size = size;
    ramdisk->buffer = kmalloc(size);

    Device device;
    memset((uint8*)&device, 0, sizeof(device));
    strcpy(device.name, devName);
    device.device_type = FT_BLOCK_DEVICE;
    device.open = open;
    device.close = close;
    device.read_block = read_block;
    device.write_block = write_block;
    device.ioctl = ioctl;
    device.private_data = ramdisk;

    if (devfs_register_device(&device))
    {
        return TRUE;
    }

    kfree(ramdisk->buffer);
    kfree(ramdisk);

    return FALSE;
}

static BOOL open(File *file, uint32 flags)
{
    return TRUE;
}

static void close(File *file)
{
}

static int32 read_block(FileSystemNode* node, uint32 block_number, uint32 count, uint8* buffer)
{
    Ramdisk* ramdisk = (Ramdisk*)node->private_node_data;

    uint32 location = block_number * RAMDISK_BLOCKSIZE;
    uint32 size = count * RAMDISK_BLOCKSIZE;

    if (location + size > ramdisk->size)
    {
        return -1;
    }

    begin_critical_section();

    memcpy(buffer, ramdisk->buffer + location, size);

    end_critical_section();

    return 0;
}

static int32 write_block(FileSystemNode* node, uint32 block_number, uint32 count, uint8* buffer)
{
    Ramdisk* ramdisk = (Ramdisk*)node->private_node_data;

    uint32 location = block_number * RAMDISK_BLOCKSIZE;
    uint32 size = count * RAMDISK_BLOCKSIZE;

    if (location + size > ramdisk->size)
    {
        return -1;
    }

    begin_critical_section();

    memcpy(ramdisk->buffer + location, buffer, size);

    end_critical_section();

    return 0;
}

static int32 ioctl(File *node, int32 request, void * argp)
{
    Ramdisk* ramdisk = (Ramdisk*)node->node->private_node_data;

    uint32* result = (uint32*)argp;

    switch (request)
    {
    case IC_GET_SECTOR_COUNT:
        *result = ramdisk->size / RAMDISK_BLOCKSIZE;
        return 0;
        break;
    case IC_GET_SECTOR_SIZE_BYTES:
        *result = RAMDISK_BLOCKSIZE;
        return 0;
        break;
    default:
        break;
    }

    return -1;
}
