/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#ifndef RANDOM_H
#define RANDOM_H

#include "stdint.h"

void random_initialize();
int32_t random_fill(uint8_t* buffer, uint32_t size);

#endif // RANDOM_H
