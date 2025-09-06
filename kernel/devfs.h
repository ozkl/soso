/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#ifndef DEVFS_H
#define DEVFS_H

#include "device.h"
#include "fs.h"
#include "common.h"

void devfs_initialize();
FileSystemNode* devfs_register_device(Device* device, BOOL add_to_fs);
void devfs_unregister_device(FileSystemNode* device_node);

#endif // DEVFS_H
