#include "devfs.h"
#include "device.h"
#include "common.h"
#include "gfx.h"
#include "framebuffer.h"
#include "vmm.h"
#include "process.h"
#include "alloc.h"

static BOOL fb_open(File *file, uint32_t flags);
static int32_t fb_read(File *file, uint32_t size, uint8_t *buffer);
static int32_t fb_write(File *file, uint32_t size, uint8_t *buffer);
static int32_t fb_ioctl(File *node, int32_t request, void * argp);
static void* fb_mmap(File* file, uint32_t size, uint32_t offset, uint32_t flags);
static BOOL fb_munmap(File* file, void* address, uint32_t size);

static uint8_t* g_fb_physical = 0;
static uint8_t* g_fb_virtual = 0;

void framebuffer_initialize(uint8_t* p_address, uint8_t* v_address)
{
    g_fb_physical = p_address;
    g_fb_virtual = v_address;

    Device device;
    memset((uint8_t*)&device, 0, sizeof(Device));
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

static BOOL fb_open(File *file, uint32_t flags)
{
    return TRUE;
}

static int32_t fb_read(File *file, uint32_t size, uint8_t *buffer)
{
    if (size == 0)
    {
        return 0;
    }

    return -1;
}

static int32_t fb_write(File *file, uint32_t size, uint8_t *buffer)
{
    if (size == 0)
    {
        return 0;
    }

    int32_t requested_size = size;

    int32_t length = gfx_get_width() * gfx_get_height() * 4;

    int32_t available_size = length - file->offset;

    if (available_size <= 0)
    {
        return -1;
    }

    int32_t target_size = MIN(requested_size, available_size);

    memcpy(g_fb_virtual + file->offset, buffer, target_size);

    file->offset += target_size;

    return target_size;
}

static int32_t fb_ioctl(File *node, int32_t request, void * argp)
{
    int32_t result = -1;

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

static void* fb_mmap(File* file, uint32_t size, uint32_t offset, uint32_t flags)
{
    uint32_t page_count = PAGE_COUNT(size);
    uint32_t* physical_pages_array = (uint32_t*)kmalloc(page_count * sizeof(uint32_t));

    //TODO: check pageCount(size). It should not exceed our framebuffer!

    for (uint32_t i = 0; i < page_count; ++i)
    {
        physical_pages_array[i] = (uint32_t)(g_fb_physical) + i * PAGESIZE_4K;
    }

    void* result = vmm_map_memory(file->thread->owner, USER_MMAP_START, physical_pages_array, page_count, FALSE);

    kfree(physical_pages_array);

    return result;
}

static BOOL fb_munmap(File* file, void* address, uint32_t size)
{
    return vmm_unmap_memory(file->thread->owner, (uint32_t)address, PAGE_COUNT(size));
}
