#ifndef GFX_H
#define GFX_H

#include "common.h"

void gfx_initialize(uint32_t* pixels, uint32_t width, uint32_t height, uint32_t bytesPerPixel, uint32_t pitch);

void gfx_put_char_at(
    /* note that this is int, not char as it's a unicode character */
    unsigned short int c,
    /* cursor position on screen, in characters not in pixels */
    int cx, int cy,
    /* foreground and background colors, say 0xFFFFFF and 0x000000 */
    uint32_t fg, uint32_t bg);



uint8_t* gfx_get_video_memory();
uint16_t gfx_get_width();
uint16_t gfx_get_height();
uint16_t gfx_get_bytes_per_pixel();
void gfx_fill(uint32_t color);

#endif // GFX_H
