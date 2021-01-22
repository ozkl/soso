#include "random.h"
#include "devfs.h"
#include "device.h"
#include "common.h"
#include "process.h"
#include "sleep.h"

static BOOL random_open(File *file, uint32_t flags);
static int32_t random_read(File *file, uint32_t size, uint8_t *buffer);

void random_initialize()
{
    Device device;
    memset((uint8_t*)&device, 0, sizeof(Device));
    strcpy(device.name, "random");
    device.device_type = FT_CHARACTER_DEVICE;
    device.open = random_open;
    device.read = random_read;

    devfs_register_device(&device);
}

static BOOL random_open(File *file, uint32_t flags)
{
    return TRUE;
}

static int32_t random_read(File *file, uint32_t size, uint8_t *buffer)
{
    if (size == 0)
    {
        return 0;
    }

    uint32_t number = rand();

    if (size == 1)
    {
        *buffer = (uint8_t)number;
        return 1;
    }
    else if (size == 2 || size == 3)
    {
        *((uint16_t*)buffer) = (uint16_t)number;
        return 2;
    }
    else if (size >= 4)
    {
        //Screen_PrintF("random_read: buffer is %x, writing %x to buffer\n", buffer, number);

        *((uint32_t*)buffer) = number;
        return 4;
    }

    return 0;
}
