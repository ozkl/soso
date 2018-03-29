#include "gfx.h"
#include "vmm.h"
#include "serial.h"
#include "debugprint.h"

static uint32 gWidth = 0;
static uint32 gHeight = 0;
static uint32 gBytePerPixel = 0;
static uint32 gPitch = 0;
static uint32* gPixels = NULL;

extern char _binary_font_psf_start;
extern char _binary_font_psf_end;

uint16 *gUnicode = NULL;

void Gfx_Initialize(uint32* pixels, uint32 width, uint32 height, uint32 bytePerPixel, uint32 pitch)
{
    char* p_address = (char*)pixels;
    char* v_address = (char*)VIDEO_MEMORY;

    //Usually physical and virtual are the same here but of course they don't have to

    gPixels = (uint32*)v_address;
    gWidth = width;
    gHeight = height;
    gBytePerPixel = bytePerPixel;
    gPitch = pitch;


    BOOL success = addPageToPd(gKernelPageDirectory, v_address, p_address, 0);

    if (success)
    {
        for (int y = 0; y < gHeight; ++y)
        {
            for (int x = 0; x < gWidth; ++x)
            {
                gPixels[x + y * gWidth] = 0xFFFFFFFF;
            }
        }
    }
    else
    {
        Debug_PrintF("Gfx initialization failed!\n");
    }
}

#define PSF_FONT_MAGIC 0x864ab572

typedef struct {
    uint32 magic;         /* magic bytes to identify PSF */
    uint32 version;       /* zero */
    uint32 headersize;    /* offset of bitmaps in file, 32 */
    uint32 flags;         /* 0 if there's no unicode table */
    uint32 numglyph;      /* number of glyphs */
    uint32 bytesperglyph; /* size of each glyph */
    uint32 height;        /* height in pixels */
    uint32 width;         /* width in pixels */
} PSF_font;

//From Osdev PC Screen Font (The font used here is free to use)
void Gfx_Putchar(
    /* note that this is int, not char as it's a unicode character */
    unsigned short int c,
    /* cursor position on screen, in characters not in pixels */
    int cx, int cy,
    /* foreground and background colors, say 0xFFFFFF and 0x000000 */
    uint32 fg, uint32 bg)
{
    /* cast the address to PSF header struct */
    PSF_font *font = (PSF_font*)&_binary_font_psf_start;
    /* we need to know how many bytes encode one row */
    int bytesperline=(font->width+7)/8;
    /* unicode translation */
    if(gUnicode != NULL) {
        c = gUnicode[c];
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
        (cy * font->height * gPitch) +
        (cx * (font->width+1) * 4);
    /* finally display pixels according to the bitmap */
    int x,y, line,mask;
    for(y=0;y<font->height;y++){
        /* save the starting position of the line */
        line=offs;
        mask=1<<(font->width-1);
        /* display a row */
        for(x=0;x<font->width;x++){
            *((uint32*)((uint8*)gPixels + line)) = ((int)*glyph) & (mask) ? fg : bg;
            /* adjust to the next pixel */
            mask >>= 1;
            line += 4;
        }
        /* adjust to the next line */
        glyph += bytesperline;
        offs  += gPitch;
    }
}
