#ifndef GFX_H
#define GFX_H

#include "common.h"

void gfx_initialize(uint32* pixels, uint32 width, uint32 height, uint32 bytesPerPixel, uint32 pitch);

void gfx_put_char_at(
    /* note that this is int, not char as it's a unicode character */
    unsigned short int c,
    /* cursor position on screen, in characters not in pixels */
    int cx, int cy,
    /* foreground and background colors, say 0xFFFFFF and 0x000000 */
    uint32 fg, uint32 bg);



uint8* gfx_get_video_memory();
uint16 gfx_get_width();
uint16 gfx_get_height();
uint16 gfx_get_bytes_per_pixel();
void gfx_fill(uint32 color);

#endif // GFX_H
