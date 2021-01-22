#include "gfx.h"
#include "vmm.h"
#include "serial.h"
#include "framebuffer.h"
#include "log.h"

static uint32_t g_width = 0;
static uint32_t g_height = 0;
static uint32_t g_bytes_per_pixel = 0;
static uint32_t g_pitch = 0;
static uint32_t* g_pixels = NULL;

extern char _binary_font_psf_start;
extern char _binary_font_psf_end;

static uint16_t *g_unicode = NULL;

#define LINE_HEIGHT 16

void gfx_initialize(uint32_t* pixels, uint32_t width, uint32_t height, uint32_t bytes_per_pixel, uint32_t pitch)
{
    uint32_t p_address = (uint32_t)pixels;
    char* v_address = (char*)GFX_MEMORY;

    //Usually physical and virtual are the same here but of course they don't have to

    g_pixels = (uint32_t*)v_address;
    g_width = width;
    g_height = height;
    g_bytes_per_pixel = bytes_per_pixel;
    g_pitch = pitch;

    uint32_t size_bytes = g_width * g_height * g_bytes_per_pixel;
    uint32_t needed_page_count = size_bytes / PAGESIZE_4K;

    for (uint32_t i = 0; i < needed_page_count; ++i)
    {
        uint32_t offset = i * PAGESIZE_4K;

        vmm_add_page_to_pd(v_address + offset, p_address + offset, 0);
    }

    for (int y = 0; y < g_height; ++y)
    {
        for (int x = 0; x < g_width; ++x)
        {
            g_pixels[x + y * g_width] = 0xFFFFFFFF;
        }
    }

    framebuffer_initialize((uint8_t*)p_address, (uint8_t*)v_address);
}

#define PSF_FONT_MAGIC 0x864ab572

typedef struct {
    uint32_t magic;         /* magic bytes to identify PSF */
    uint32_t version;       /* zero */
    uint32_t headersize;    /* offset of bitmaps in file, 32 */
    uint32_t flags;         /* 0 if there's no unicode table */
    uint32_t numglyph;      /* number of glyphs */
    uint32_t bytesperglyph; /* size of each glyph */
    uint32_t height;        /* height in pixels */
    uint32_t width;         /* width in pixels */
} PSF_font;

//From Osdev PC Screen Font (The font used here is free to use)
void gfx_put_char_at(
    /* note that this is int, not char as it's a unicode character */
    unsigned short int c,
    /* cursor position on screen, in characters not in pixels */
    int cx, int cy,
    /* foreground and background colors, say 0xFFFFFF and 0x000000 */
    uint32_t fg, uint32_t bg)
{
    /* cast the address to PSF header struct */
    PSF_font *font = (PSF_font*)&_binary_font_psf_start;
    /* we need to know how many bytes encode one row */
    int bytesperline=(font->width+7)/8;
    /* unicode translation */
    if(g_unicode != NULL) {
        c = g_unicode[c];
    }
    /* get the glyph for the character. If there's no
       glyph for a given character, we'll display the first glyph. */
    unsigned char *glyph =
     (unsigned char*)&_binary_font_psf_start +
     font->headersize +
     (c>0&&c<font->numglyph?c:0)*font->bytesperglyph;
    /* calculate the upper left corner on screen where we want to display.
       we only do this once, and adjust the offset later. This is faster. */
    int offs =
        (cy * font->height * g_pitch) +
        (cx * (font->width+1) * 4);
    /* finally display pixels according to the bitmap */
    int x,y, line,mask;
    for(y=0;y<font->height;y++){
        /* save the starting position of the line */
        line=offs;
        mask=1<<(font->width-1);
        /* display a row */
        for(x=0;x<font->width;x++)
        {
            if (c == 0)
            {
                *((uint32_t*)((uint8_t*)g_pixels + line)) = bg;
            }
            else
            {
                *((uint32_t*)((uint8_t*)g_pixels + line)) = ((int)*glyph) & (mask) ? fg : bg;
            }

            /* adjust to the next pixel */
            mask >>= 1;
            line += 4;
        }
        /* adjust to the next line */
        glyph += bytesperline;
        offs  += g_pitch;
    }
}

uint8_t* gfx_get_video_memory()
{
    return (uint8_t*)g_pixels;
}

uint16_t gfx_get_width()
{
    return g_width;
}

uint16_t gfx_get_height()
{
    return g_height;
}

uint16_t gfx_get_bytes_per_pixel()
{
    return g_bytes_per_pixel;
}

void gfx_fill(uint32_t color)
{
    for (uint32_t y = 0; y < g_height; ++y)
    {
        for (uint32_t x = 0; x < g_width; ++x)
        {
            g_pixels[x + y * g_width] = color;
        }
    }
}
