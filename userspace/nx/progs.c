#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#define MWINCLUDECOLORS
#include "nano-X.h"
#include "nxdraw.h"

extern char **environ;

GR_WINDOW_ID	g_window_id;
GR_GC_ID		g_graphics_context;
GR_FONT_ID   	g_font;

GR_SCREEN_INFO	g_screen_info;
GR_FONT_INFO    g_font_info;


GR_WINDOW_INFO  g_window_info;
GR_GC_INFO  g_gc_info;

#define BUTTON_WIDTH 60

void start_term()
{
	int pid = fork();
	if (pid == 0)
	{
		char* argv[2];
		argv[0] = "/initrd/term";
		argv[1] = NULL;
		execv(argv[0], argv);

		//if returns, it means it cannot start the process so exiting the child.
		exit(1);
	}
}

void start_doom()
{
	int pid = fork();
	if (pid == 0)
	{
		chdir("/initrd");

		char* argv[2];
		argv[0] = "/initrd/doom";
		argv[1] = NULL;
		execv(argv[0], argv);

		//if returns, it means it cannot start the process so exiting the child.
		exit(1);
	}
}

int main(int argc, char **argv)
{
    GR_WM_PROPERTIES props;

    if (GrOpen() < 0)
	{
		GrError("cannot open graphics\n");
		exit(1);
    }

    GrGetScreenInfo(&g_screen_info);

    g_font = GrCreateFontEx(GR_FONT_SYSTEM_FIXED, 0, 0, NULL);

    GrGetFontInfo(g_font, &g_font_info);
    int winw = 200;
    int winh = 80;

    g_window_id = GrNewWindow(GR_ROOT_WINDOW_ID, 600, 10, winw, winh, 0, LTGRAY, BLACK);

    props.title = "Programs";
    props.flags = GR_WM_FLAGS_TITLE | GR_WM_FLAGS_PROPS;
	props.props = GR_WM_PROPS_CAPTION;
    GrSetWMProperties(g_window_id, &props);

    GrSelectEvents(g_window_id, GR_EVENT_MASK_BUTTON_DOWN | GR_EVENT_MASK_EXPOSURE |
		   GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_CLOSE_REQ);
    GrMapWindow(g_window_id);

	GrMoveWindow(g_window_id, 600, 10);

    GrGetWindowInfo(g_window_id,&g_window_info);

    g_graphics_context = GrNewGC();
    GrSetGCFont(g_graphics_context, g_font);

    GrSetGCForeground(g_graphics_context, BLACK);
    GrSetGCBackground(g_graphics_context, GRAY);
    GrGetGCInfo(g_graphics_context, &g_gc_info);

	GR_WM_PROPERTIES props_button;
	props_button.flags = GR_WM_FLAGS_PROPS;
	props_button.props = GR_WM_PROPS_CAPTION;

	GR_WINDOW_ID button_terminal = GrNewWindow(g_window_id, 10, 10, BUTTON_WIDTH, 20, 0, GRAY, BLACK);
	GrSetWMProperties(button_terminal, &props_button);
	GrMapWindow(button_terminal);
	GrSelectEvents(button_terminal, GR_EVENT_MASK_BUTTON_DOWN | GR_EVENT_MASK_BUTTON_UP | GR_EVENT_MASK_EXPOSURE);

	GR_WINDOW_ID button_doom = GrNewWindow(g_window_id, 80, 10, BUTTON_WIDTH, 20, 0, GRAY, BLACK);
	GrSetWMProperties(button_doom, &props_button);
	GrMapWindow(button_doom);
	GrSelectEvents(button_doom, GR_EVENT_MASK_BUTTON_DOWN | GR_EVENT_MASK_BUTTON_UP | GR_EVENT_MASK_EXPOSURE);


	GR_EVENT wevent;
	GR_EVENT_KEYSTROKE *kp = NULL;
	GR_EVENT_BUTTON *button_event = NULL;
	GR_EVENT_EXPOSURE *exp_event = NULL;
	while (1)
	{
		GrGetNextEvent(&wevent);

		switch(wevent.type)
		{
		case GR_EVENT_TYPE_CLOSE_REQ:
			//GrClose();
			//exit(0);
			break;
		case GR_EVENT_TYPE_EXPOSURE:
			exp_event = (GR_EVENT_EXPOSURE*)&wevent;
			if (exp_event->wid == button_terminal)
			{
				nxDraw3dUpDownState(button_terminal, 0, 0, BUTTON_WIDTH, 20, FALSE);
				GrText(button_terminal, g_graphics_context, 6, 15, "Terminal", -1, GR_TFASCII);
			}
			else if (exp_event->wid == button_doom)
			{
				nxDraw3dUpDownState(button_doom, 0, 0, BUTTON_WIDTH, 20, FALSE);
				GrText(button_doom, g_graphics_context, 18, 15, "Doom", -1, GR_TFASCII);
			}
			break;

		case GR_EVENT_TYPE_KEY_DOWN:
			kp = (GR_EVENT_KEYSTROKE *)&wevent;
			
			break;
		case GR_EVENT_TYPE_BUTTON_DOWN:
			button_event = (GR_EVENT_BUTTON*)&wevent;
			if (button_event->wid == button_terminal)
			{
				nxDraw3dUpDownState(button_terminal, 0, 0, BUTTON_WIDTH, 20, TRUE);
				start_term();
			}
			else if (button_event->wid == button_doom)
			{
				nxDraw3dUpDownState(button_doom, 0, 0, BUTTON_WIDTH, 20, TRUE);
				start_doom();
			}
			break;
		case GR_EVENT_TYPE_BUTTON_UP:
			button_event = (GR_EVENT_BUTTON*)&wevent;
			if (button_event->wid == button_terminal || button_event->wid == button_doom)
			{
				nxDraw3dUpDownState(button_event->wid, 0, 0, BUTTON_WIDTH, 20, FALSE);
			}
			break;
		break;
		}

	}
    return 0;
}