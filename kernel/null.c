#include "null.h"
#include "devfs.h"
#include "device.h"
#include "common.h"

static BOOL null_open(File *file, uint32_t flags);

void null_initialize()
{
    Device device;
    memset((uint8_t*)&device, 0, sizeof(Device));
    strcpy(device.name, "null");
    device.device_type = FT_CHARACTER_DEVICE;
    device.open = null_open;

    devfs_register_device(&device);
}

static BOOL null_open(File *file, uint32_t flags)
{
    return TRUE;
}
