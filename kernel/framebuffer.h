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
