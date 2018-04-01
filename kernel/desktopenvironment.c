#include "desktopenvironment.h"
#include "alloc.h"
#include "gfx.h"

static uint16 gTitleBarHeight = 20;

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
    uint8* buffer;
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
    de->buffer = kmalloc(width * height * 4);
    memset(de->buffer, 0, sizeof(width * height * 4));
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

void DE_SetWindowPosition(Window* window, uint16 x, uint16 y)
{
    window->x = x;
    window->y = y;
}

void DE_MoveWindowToTop(Window* window)
{
    ListNode* node = List_FindFirstOccurrence(window->desktopEnvironment->windows, window);

    if (node)
    {
        List_RemoveNode(window->desktopEnvironment->windows, node);

        List_Append(window->desktopEnvironment->windows, window);
    }
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
    uint8* videoMemory = Gfx_GetVideoMemory();

    uint32* backBuffer = (uint32*)de->buffer;

    //Background
    memset((uint8*)backBuffer, 0x99, de->width * de->height * 4);

    List_Foreach(n, de->windows)
    {
        Window* window = (Window*)n->data;

        for (int y = 0; y < window->height + gTitleBarHeight; ++y)
        {
            uint32* lineStart = backBuffer + (window->y + y) * de->width + window->x;

            if (y < gTitleBarHeight) //titleBar
            {
                for (int x = 0; x < window->width; ++x)
                {
                    lineStart[x] = 0x000094FF;
                }
            }
            else
            {
                uint8* windowLine = window->buffer + (y - gTitleBarHeight) * window->width * 4;

                memcpy((uint8*)lineStart, windowLine, window->width * 4);
            }
        }
    }

    memcpy(videoMemory, (uint8*)backBuffer, de->width * de->height * 4);
}
