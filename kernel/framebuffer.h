/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "common.h"

enum EnFrameBuferIoctl
{
    FB_GET_WIDTH,
    FB_GET_HEIGHT,
    FB_GET_BITSPERPIXEL
};

void framebuffer_initialize(uint8_t* p_address, uint8_t* v_address);

#endif // FRAMEBUFFER_H
