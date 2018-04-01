#include "desktopenvironment.h"
#include "alloc.h"
#include "gfx.h"

typedef struct DesktopEnvironment DesktopEnvironment;

typedef struct Window
{
    uint16 width;
    uint16 height;
    int16 x;
    int16 y;
    uint8* buffer;
    Thread* thread;
    DesktopEnvironment* desktopEnvironment;
    char title[32];
} Window;

typedef struct DesktopEnvironment
{
    uint16 width;
    uint16 height;
    List* windows;
} DesktopEnvironment;

static DesktopEnvironment* gDefaultDesktopEnvironment = NULL;

DesktopEnvironment* DE_GetDefault()
{
    return gDefaultDesktopEnvironment;
}

void DE_SetDefault(DesktopEnvironment* de)
{
    gDefaultDesktopEnvironment = de;
}

DesktopEnvironment* DE_Create(uint16 width, uint16 height)
{
    DesktopEnvironment* de = (DesktopEnvironment*)kmalloc(sizeof(DesktopEnvironment));
    memset((uint8*)de, 0, sizeof(DesktopEnvironment));
    de->width = width;
    de->height = height;
    de->windows = List_Create();

    return de;
}

Window* DE_CreateWindow(DesktopEnvironment* de, uint16 width, uint16 height, Thread* ownerThread)
{
    Window* window = (Window*)kmalloc(sizeof(Window));
    memset((uint8*)window, 0, sizeof(Window));
    window->width = width;
    window->height = height;
    window->thread = ownerThread;
    window->desktopEnvironment = de;
    window->buffer = kmalloc(width * height * 4);
    memset(window->buffer, 0, sizeof(width * height * 4));

    List_Append(de->windows, window);

    return window;
}

void DE_DestroyWindow(Window* window)
{
    List_RemoveFirstOccurrence(window->desktopEnvironment->windows, window);

    kfree(window->buffer);
    kfree(window);
}

uint16 DE_GetWidth(DesktopEnvironment* de)
{
    return de->width;
}

uint16 DE_GetHeight(DesktopEnvironment* de)
{
    return de->height;
}

void DE_Update(DesktopEnvironment* de)
{
    uint32* videoMemory = (uint32*)Gfx_GetVideoMemory();

    Gfx_Fill(0xFF999999);

    List_Foreach(n, de->windows)
    {
        Window* window = (Window*)n->data;

        for (int y = 0; y < window->height; ++y)
        {
            uint8* line = window->buffer + y * window->width * 4;

            memcpy((uint8*)(videoMemory + window->y * window->width + window->x), line, window->width * 4);
        }
    }
}
