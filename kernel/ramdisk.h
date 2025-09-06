/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#ifndef RAMDISK_H
#define RAMDISK_H

#include "common.h"

BOOL ramdisk_create(const char* devName, uint32_t size, uint8_t* preallocated_buffer);

#endif // RAMDISK_H
