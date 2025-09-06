/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#ifndef DEVICE_H
#define DEVICE_H

#include "common.h"
#include "fs.h"

typedef struct Device
{
    char name[16];
    FileType device_type;
    ReadWriteBlockFunction read_block;
    ReadWriteBlockFunction write_block;
    ReadWriteFunction read;
    ReadWriteFunction write;
    ReadWriteTestFunction read_test_ready;
    ReadWriteTestFunction write_test_ready;
    OpenFunction open;
    CreateFunction create;
    CloseFunction close;
    IoctlFunction ioctl;
    FtruncateFunction ftruncate;
    MmapFunction mmap;
    MunmapFunction munmap;
    void * private_data;
} Device;

#endif // DEVICE_H
