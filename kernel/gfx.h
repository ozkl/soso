#ifndef GFX_H
#define GFX_H

#include "common.h"

void Gfx_Initialize(uint32* pixels, uint32 width, uint32 height, uint32 bytePerPixel, uint32 pitch);

void Gfx_PutCharAt(
    /* note that this is int, not char as it's a unicode character */
    unsigned short int c,
    /* cursor position on screen, in characters not in pixels */
    int cx, int cy,
    /* foreground and background colors, say 0xFFFFFF and 0x000000 */
    uint32 fg, uint32 bg);

#endif // GFX_H
