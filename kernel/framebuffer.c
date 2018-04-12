#include "devfs.h"
#include "device.h"
#include "common.h"
#include "gfx.h"
#include "framebuffer.h"

static BOOL fb_open(File *file, uint32 flags);
static int32 fb_read(File *file, uint32 size, uint8 *buffer);
static int32 fb_write(File *file, uint32 size, uint8 *buffer);
static int32 fb_ioctl(File *node, int32 request, void * argp);

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
    //TODO:device.mmap

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

    //TODO:

    return 0;
}

static int32 fb_write(File *file, uint32 size, uint8 *buffer)
{
    if (size == 0)
    {
        return 0;
    }

    //TODO:

    return 0;
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
