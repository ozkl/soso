/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

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

    devfs_register_device(&device, TRUE);
}

static BOOL random_open(File *file, uint32_t flags)
{
    return TRUE;
}

static int32_t random_read(File *file, uint32_t size, uint8_t *buffer)
{
    return random_fill(buffer, size);
}

int32_t random_fill(uint8_t* buffer, uint32_t size)
{
    uint32_t offset = 0;

    while (offset < size)
    {
        uint32_t number = rand();

        for (uint32_t byte = 0; byte < sizeof(number) && offset < size; ++byte)
        {
            buffer[offset++] = (uint8_t)(number >> (byte * 8));
        }
    }

    return size;
}
