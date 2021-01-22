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

static uint8* g_fb_physical = 0;
static uint8* g_fb_virtual = 0;

void framebuffer_initialize(uint8* p_address, uint8* v_address)
{
    g_fb_physical = p_address;
    g_fb_virtual = v_address;

    Device device;
    memset((uint8*)&device, 0, sizeof(Device));
    strcpy(device.name, "fb0");
    device.device_type = FT_CHARACTER_DEVICE;
    device.open = fb_open;
    device.read = fb_read;
    device.write = fb_write;
    device.ioctl = fb_ioctl;
    device.mmap = fb_mmap;
    device.munmap = fb_munmap;

    devfs_register_device(&device);
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

    int32 requested_size = size;

    int32 length = gfx_get_width() * gfx_get_height() * 4;

    int32 available_size = length - file->offset;

    if (available_size <= 0)
    {
        return -1;
    }

    int32 target_size = MIN(requested_size, available_size);

    memcpy(g_fb_virtual + file->offset, buffer, target_size);

    file->offset += target_size;

    return target_size;
}

static int32 fb_ioctl(File *node, int32 request, void * argp)
{
    int32 result = -1;

    switch (request)
    {
    case FB_GET_WIDTH:
        result = gfx_get_width();
        break;
    case FB_GET_HEIGHT:
        result = gfx_get_height();
        break;
    case FB_GET_BITSPERPIXEL:
        result = 32;
        break;
    }

    return result;
}

static void* fb_mmap(File* file, uint32 size, uint32 offset, uint32 flags)
{
    uint32 page_count = PAGE_COUNT(size);
    uint32* physical_pages_array = (uint32*)kmalloc(page_count * sizeof(uint32));

    //TODO: check pageCount(size). It should not exceed our framebuffer!

    for (uint32 i = 0; i < page_count; ++i)
    {
        physical_pages_array[i] = (uint32)(g_fb_physical) + i * PAGESIZE_4K;
    }

    void* result = vmm_map_memory(file->thread->owner, USER_MMAP_START, physical_pages_array, page_count, FALSE);

    kfree(physical_pages_array);

    return result;
}

static BOOL fb_munmap(File* file, void* address, uint32 size)
{
    return vmm_unmap_memory(file->thread->owner, (uint32)address, PAGE_COUNT(size));
}
