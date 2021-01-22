#include "ramdisk.h"
#include "alloc.h"
#include "fs.h"
#include "devfs.h"

typedef struct Ramdisk
{
    uint8_t* buffer;
    uint32_t size;
} Ramdisk;

#define RAMDISK_BLOCKSIZE 512

static BOOL open(File *file, uint32_t flags);
static void close(File *file);
static int32_t read_block(FileSystemNode* node, uint32_t block_number, uint32_t count, uint8_t* buffer);
static int32_t write_block(FileSystemNode* node, uint32_t block_number, uint32_t count, uint8_t* buffer);
static int32_t ioctl(File *node, int32_t request, void * argp);

BOOL ramdisk_create(const char* devName, uint32_t size)
{
    Ramdisk* ramdisk = kmalloc(sizeof(Ramdisk));
    ramdisk->size = size;
    ramdisk->buffer = kmalloc(size);

    Device device;
    memset((uint8_t*)&device, 0, sizeof(device));
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

static BOOL open(File *file, uint32_t flags)
{
    return TRUE;
}

static void close(File *file)
{
}

static int32_t read_block(FileSystemNode* node, uint32_t block_number, uint32_t count, uint8_t* buffer)
{
    Ramdisk* ramdisk = (Ramdisk*)node->private_node_data;

    uint32_t location = block_number * RAMDISK_BLOCKSIZE;
    uint32_t size = count * RAMDISK_BLOCKSIZE;

    if (location + size > ramdisk->size)
    {
        return -1;
    }

    begin_critical_section();

    memcpy(buffer, ramdisk->buffer + location, size);

    end_critical_section();

    return 0;
}

static int32_t write_block(FileSystemNode* node, uint32_t block_number, uint32_t count, uint8_t* buffer)
{
    Ramdisk* ramdisk = (Ramdisk*)node->private_node_data;

    uint32_t location = block_number * RAMDISK_BLOCKSIZE;
    uint32_t size = count * RAMDISK_BLOCKSIZE;

    if (location + size > ramdisk->size)
    {
        return -1;
    }

    begin_critical_section();

    memcpy(ramdisk->buffer + location, buffer, size);

    end_critical_section();

    return 0;
}

static int32_t ioctl(File *node, int32_t request, void * argp)
{
    Ramdisk* ramdisk = (Ramdisk*)node->node->private_node_data;

    uint32_t* result = (uint32_t*)argp;

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
