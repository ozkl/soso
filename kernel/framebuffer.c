#include "devfs.h"
#include "device.h"
#include "common.h"
#include "gfx.h"
#include "framebuffer.h"
#include "vmm.h"
#include "process.h"
#include "alloc.h"

static BOOL fb_open(File *file, uint32 flags);
static int32 fb_read(File *file, uint32 size, uint8 *buffer);
static int32 fb_write(File *file, uint32 size, uint8 *buffer);
static int32 fb_ioctl(File *node, int32 request, void * argp);
static void* fb_mmap(File* file, uint32 size, uint32 offset, uint32 flags);
static BOOL fb_munmap(File* file, void* address, uint32 size);

static uint8* gFrameBufferPhysical = 0;
static uint8* gFrameBufferVirtual = 0;

void initializeFrameBuffer(uint8* p_address, uint8* v_address)
{
    gFrameBufferPhysical = p_address;
    gFrameBufferVirtual = v_address;

    Device device;
    memset((uint8*)&device, 0, sizeof(Device));
    strcpy(device.name, "fb0");
    device.deviceType = FT_CharacterDevice;
    device.open = fb_open;
    device.read = fb_read;
    device.write = fb_write;
    device.ioctl = fb_ioctl;
    device.mmap = fb_mmap;
    device.munmap = fb_munmap;

    registerDevice(&device);
}

static BOOL fb_open(File *file, uint32 flags)
{
    return TRUE;
}

static int32 fb_read(File *file, uint32 size, uint8 *buffer)
{
    if (size == 0)
    {
        return 0;
    }

    return -1;
}

static int32 fb_write(File *file, uint32 size, uint8 *buffer)
{
    if (size == 0)
    {
        return 0;
    }

    int32 requestedSize = size;

    int32 length = Gfx_GetWidth() * Gfx_GetHeight() * 4;

    int32 availableSize = length - file->offset;

    if (availableSize <= 0)
    {
        return -1;
    }

    int32 targetSize = MIN(requestedSize, availableSize);

    memcpy(gFrameBufferVirtual + file->offset, buffer, targetSize);

    file->offset += targetSize;

    return targetSize;
}

static int32 fb_ioctl(File *node, int32 request, void * argp)
{
    int32 result = -1;

    switch (request)
    {
    case FB_GET_WIDTH:
        result = Gfx_GetWidth();
        break;
    case FB_GET_HEIGHT:
        result = Gfx_GetHeight();
        break;
    case FB_GET_BITSPERPIXEL:
        result = 32;
        break;
    }

    return result;
}

static void* fb_mmap(File* file, uint32 size, uint32 offset, uint32 flags)
{
    uint32 pageCount = PAGE_COUNT(size);
    uint32* physicalPagesArray = (uint32*)kmalloc(pageCount * sizeof(uint32));

    //TODO: check pageCount(size). It should not exceed our framebuffer!

    for (uint32 i = 0; i < pageCount; ++i)
    {
        physicalPagesArray[i] = (uint32)(gFrameBufferPhysical) + i * PAGESIZE_4K;
    }

    void* result = mapMemory(file->thread->owner, USER_MMAP_START, physicalPagesArray, pageCount, FALSE);

    kfree(physicalPagesArray);

    return result;
}

static BOOL fb_munmap(File* file, void* address, uint32 size)
{
    return unmapMemory(file->thread->owner, (uint32)address, PAGE_COUNT(size));
}
