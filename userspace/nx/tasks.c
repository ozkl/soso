#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>

#include <nano-X.h>

#include <soso.h>

static int get_process_cpu_usage(uint32_t pid, ThreadInfo* threads, uint32_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        ThreadInfo* info = threads + i;

        if (info->process_id == pid)
        {
            return info->usage_cpu;
        }
    }
    
    return 0;
}

GR_WINDOW_ID  wid;
GR_GC_ID      gc;

const int winSizeX = 300;
const int winSizeY = 300;
unsigned char* windowBuffer = 0;

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
//#define NK_XLIBSHM_IMPLEMENTATION
#define NK_RAWFB_IMPLEMENTATION
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_SOFTWARE_FONT

#include "nuklear.h"
#include "nuklear_rawfb.h"
struct rawfb_context *rawfb;
unsigned char tex_scratch[512 * 512];

void handle_nuklear_input(GR_EVENT* event)
{
    if (event)
    {
        int x = event->mouse.x;
        int y = event->mouse.y;
        switch (event->type)
        {
        case GR_EVENT_TYPE_MOUSE_POSITION:
            nk_input_motion(&rawfb->ctx, x, y);
            break;
        case GR_EVENT_TYPE_BUTTON_DOWN:
            nk_input_motion(&rawfb->ctx, x, y);
            nk_input_button(&rawfb->ctx, NK_BUTTON_LEFT, x, y, 1);
            break;
        case GR_EVENT_TYPE_BUTTON_UP:
            nk_input_motion(&rawfb->ctx, x, y);
            nk_input_button(&rawfb->ctx, NK_BUTTON_LEFT, x, y, 0);
            break;
        }
    }
}

void frame(GR_EVENT* event)
{
    ProcInfo procs[30];
    int process_count = getprocs(procs, 20, 0);


    ThreadInfo threads[30];
    int thread_count = getthreads(threads, 20, 0);

    char buffer[64];

    struct nk_context* ctx = &rawfb->ctx;

    nk_input_begin(ctx);
    handle_nuklear_input(event);
    nk_input_end(ctx);

    if (nk_begin(ctx, "Win1", nk_rect(0, 0, winSizeX, winSizeY), 0))
    {
        nk_layout_row_dynamic(ctx, 22, 1);
        sprintf(buffer, "Count: %d", process_count);
        nk_label_colored(ctx, buffer, NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE, nk_rgb(255, 0, 0));

        nk_layout_row_dynamic(ctx, 22, 4);
        nk_label_colored(ctx, "Id", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE, nk_rgb(255, 0, 0));
        nk_label_colored(ctx, "Name", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE, nk_rgb(255, 0, 0));
        nk_label_colored(ctx, "CPU (%)", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE, nk_rgb(255, 0, 0));
        nk_label_colored(ctx, "Kill", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE, nk_rgb(255, 0, 0));

        sprintf(buffer, "-");

        for (int i = 0; i < process_count; ++i)
        {
            ProcInfo* p = procs + i;

            uint32_t usage = get_process_cpu_usage(p->process_id, threads, thread_count);

            nk_layout_row_dynamic(ctx, 22, 4);
            
            sprintf(buffer, "%d", p->process_id);
            nk_label(ctx, buffer, NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
            
            nk_label(ctx, p->name, NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
            
            sprintf(buffer, "%d", usage);
            nk_label(ctx, buffer, NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
            
            if (nk_button_label(ctx, "Kill"))
            {
                kill(p->process_id, SIGTERM);
            }
        }
    }
    nk_end(ctx);
    if (nk_window_is_closed(ctx, "Win1"))
    {
        //printf("nk close window\n");
        exit(0);
    }

    nk_rawfb_render(rawfb, nk_rgb(30, 30, 30), 1);
}

int main(int argc, char** argv)
{
    if (GrOpen() < 0)
    {
        GrError("GrOpen failed");
        return 1;
    }

    gc = GrNewGC();
    GrSetGCUseBackground(gc, GR_FALSE);
    GrSetGCForeground(gc, MWRGB( 255, 255, 0 ));

    wid = GrNewBufferedWindow(GR_WM_PROPS_APPFRAME |
                        GR_WM_PROPS_CAPTION  |
                        GR_WM_PROPS_CLOSEBOX |
                        GR_WM_PROPS_BUFFER_MMAP |
                        GR_WM_PROPS_BUFFER_BGRA, //actually it is ABGR
                        "Task Manager",
                        GR_ROOT_WINDOW_ID, 
                        50, 50, winSizeX, winSizeY, MWRGB( 255, 255, 255 ));

    GrSelectEvents(wid, GR_EVENT_MASK_EXPOSURE | 
                        GR_EVENT_MASK_TIMER |
                        GR_EVENT_MASK_CLOSE_REQ |
                        GR_EVENT_MASK_BUTTON_DOWN |
                        GR_EVENT_MASK_BUTTON_UP |
                        GR_EVENT_MASK_MOUSE_POSITION);

    GrMapWindow (wid);

    windowBuffer = GrOpenClientFramebuffer(wid);

    GrCreateTimer(wid, 1000);

    rawfb = nk_rawfb_init(windowBuffer, tex_scratch, winSizeX, winSizeY, winSizeX * 4, PIXEL_LAYOUT_XRGB_8888);

    nk_style_hide_cursor(&rawfb->ctx);

    frame(NULL);
    GrFlushWindow(wid);

    GR_EVENT event;
    while (1)
    {
        GrGetNextEventTimeout(&event, 5000);

        switch (event.type)
        {
        case GR_EVENT_TYPE_BUTTON_DOWN:
        case GR_EVENT_TYPE_BUTTON_UP:
        case GR_EVENT_TYPE_MOUSE_POSITION:
            frame(&event);
            break;

        case GR_EVENT_TYPE_CLOSE_REQ:
            GrClose();
            exit (0);
            break;
        case GR_EVENT_TYPE_EXPOSURE:
            
            break;
        case GR_EVENT_TYPE_TIMER:
            frame(&event);
            GrFlushWindow(wid);
            break;
        }
    }
    
    nk_rawfb_shutdown(rawfb);

    GrCloseClientFramebuffer(wid);

    return 0;
}
