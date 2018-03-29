#include "gfx.h"
#include "vmm.h"
#include "serial.h"

static uint32 gWidth = 0;
static uint32 gHeight = 0;
static uint32 gBytePerPixel = 0;
static uint32* gPixels = NULL;

void Gfx_Initialize(uint32* pixels, uint32 width, uint32 height, uint32 bytePerPixel)
{
    gPixels = pixels;
    gWidth = width;
    gHeight = height;
    gBytePerPixel = bytePerPixel;

    Serial_PrintF("Gfx_Initialize\n");

    char* p_address = (char*)gPixels;
    char* v_address = (char*)gPixels;

    Serial_PrintF("Gfx_Initialize: %x\n", gPixels);

    BOOL success = addPageToPd(gKernelPageDirectory, v_address, p_address, 0);

    if (success)
    {
        Serial_PrintF("Gfx_Initialize: success\n");

        for (int y = 0; y < gHeight; ++y)
        {
            for (int x = 0; x < gWidth; ++x)
            {
                gPixels[x + y * gWidth] = 255;
            }
        }
    }

    Serial_PrintF("Gfx_Initialize: end\n");
}
