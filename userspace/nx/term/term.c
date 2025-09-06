/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2025, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <termios.h>

#define MWINCLUDECOLORS
#include "nano-X.h"

#include "ozterm.h"

static void terminal_move_cursor(Ozterm* term, int16_t old_row, int16_t old_column, int16_t row, int16_t column);
static void draw_scrollbar(Ozterm* term);


#define stdforeground WHITE
#define stdbackground BLACK
#define COLS 80
#define ROWS 25

#define SCROLLBAR_WIDTH 4
#define SCROLLBAR_MARGIN 2

#define TITLE		"Terminal"

extern char **environ;

GR_WINDOW_ID	g_window_id;
GR_GC_ID		g_graphics_context;
GR_FONT_ID   	g_font;

GR_SCREEN_INFO	g_screen_info;
GR_FONT_INFO    g_font_info;


GR_WINDOW_INFO  g_window_info;
GR_GC_INFO  g_gc_info;

typedef struct Terminal
{
    Ozterm * term;
    int scrollbar_dragging;
    int scrollbar_drag_start_y;
    int scrollbar_scroll_start_offset;
} Terminal;

static Terminal* g_terminal = NULL;
static int g_master_fd = -1;

static uint32_t GetTicksMs()
{
    struct timeval  tp;
    struct timezone tzp;

    gettimeofday(&tp, &tzp);

    return (tp.tv_sec * 1000) + (tp.tv_usec / 1000); /* return milliseconds */
}


int execute_on_tty(const char *path, char *const argv[], char *const envp[], const char *tty_path)
{
	return syscall(3008, path, argv, envp, tty_path);
}

int posix_openpt_(int flags)
{
	return syscall(3001, flags);
}

int ptsname_r_(int fd, char *buf, int buflen)
{
	return syscall(3002, fd, buf, buflen);
}


int term_init(void)
{
	int tfd;
	char ptyname[50];
	
	tfd = posix_openpt_(O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (tfd < 0)
	{
		GrError("Can't create pty\n");
		return -1;
	}

    ptsname_r_(tfd, ptyname, 50);

	char* argv[2];
    argv[0] = "/initrd/shell";
    argv[1] = NULL;

    execute_on_tty(argv[0], argv, environ, ptyname);

	return tfd;	
}

static void window_put_text(int row, int column, char *text)
{
	if (row >= 0 && row < ROWS && column >= 0 && column < COLS)
	{
		GrText(g_window_id, g_graphics_context, column * g_font_info.maxwidth, (row + 1) * g_font_info.height, text, -1, GR_TFASCII);
	}
}

static void window_refresh(Ozterm* term)
{
	char line[256];
	
	int16_t row_count = ozterm_get_row_count(term);
    int16_t column_count = ozterm_get_column_count(term);

    for (int y = 0; y < row_count; ++y)
    {
        OztermCell* row = ozterm_get_row_data(term, y);

        for (int x = 0; x < column_count; ++x)
        {
            OztermCell* cell = row + x;
            
			line[x] = cell->character;
        }
		line[column_count] = 0;

		window_put_text(y, 0, line);
    }

	int16_t scroll = ozterm_get_scroll(term);
	
	draw_scrollbar(term);

	if (scroll == 0)
	{
		int16_t cursor_row = ozterm_get_cursor_row(term);
		int16_t cursor_column = ozterm_get_cursor_column(term);
		terminal_move_cursor(term, cursor_row, cursor_column, cursor_row, cursor_column);
	}
}

void handle_key_press(GR_EVENT_KEYSTROKE *kp)
{
	int wkey = kp->ch;
	int wmodifier = kp->modifiers;

	//printf("keypress:%d\n", wkey);

	uint8_t modifier = 0;
	int terminal_key = OZTERM_KEY_NONE;

	if (wmodifier & MWKMOD_LSHIFT) modifier |= OZTERM_KEYM_LEFTSHIFT;
	if (wmodifier & MWKMOD_RSHIFT) modifier |= OZTERM_KEYM_RIGHTSHIFT;
	if (wmodifier & MWKMOD_CTRL) modifier |= OZTERM_KEYM_CTRL;
	if (wmodifier & MWKMOD_ALT ) modifier |= OZTERM_KEYM_ALT;

	switch (wkey)
	{
	case MWKEY_ENTER:   terminal_key = OZTERM_KEY_RETURN; break;
	case MWKEY_BACKSPACE:terminal_key = OZTERM_KEY_BACKSPACE; break;
	case MWKEY_ESCAPE:   terminal_key = OZTERM_KEY_ESCAPE; break;
	case MWKEY_TAB:      terminal_key = OZTERM_KEY_TAB; break;
	case MWKEY_DOWN: case MWKEY_KP2:    terminal_key = OZTERM_KEY_DOWN; break;
	case MWKEY_UP: case MWKEY_KP8:      terminal_key = OZTERM_KEY_UP; break;
	case MWKEY_LEFT: case MWKEY_KP4:    terminal_key = OZTERM_KEY_LEFT; break;
	case MWKEY_RIGHT: case MWKEY_KP6:   terminal_key = OZTERM_KEY_RIGHT; break;
	case MWKEY_HOME: case MWKEY_KP7:    terminal_key = OZTERM_KEY_HOME; break;
	case MWKEY_END:  case MWKEY_KP1:    terminal_key = OZTERM_KEY_END; break;
	case MWKEY_PAGEUP: case MWKEY_KP9:  terminal_key = OZTERM_KEY_PAGEUP; break;
	case MWKEY_PAGEDOWN: case MWKEY_KP3: terminal_key = OZTERM_KEY_PAGEDOWN; break;
	case MWKEY_F1:  terminal_key = OZTERM_KEY_F1; break;
	case MWKEY_F2:  terminal_key = OZTERM_KEY_F2; break;
	case MWKEY_F3:  terminal_key = OZTERM_KEY_F3; break;
	case MWKEY_F4:  terminal_key = OZTERM_KEY_F4; break;
	case MWKEY_F5:  terminal_key = OZTERM_KEY_F5; break;
	case MWKEY_F6:  terminal_key = OZTERM_KEY_F6; break;
	case MWKEY_F7:  terminal_key = OZTERM_KEY_F7; break;
	case MWKEY_F8:  terminal_key = OZTERM_KEY_F8; break;
	case MWKEY_F9:  terminal_key = OZTERM_KEY_F9; break;
	case MWKEY_F10: terminal_key = OZTERM_KEY_F10; break;
	case MWKEY_F11: terminal_key = OZTERM_KEY_F11; break;
	case MWKEY_F12: terminal_key = OZTERM_KEY_F12; break;
	
	default:
		terminal_key = wkey;
		break;
	}

	if (terminal_key != OZTERM_KEY_NONE)
	{
		if (terminal_key == MWKEY_LSHIFT || terminal_key == MWKEY_RSHIFT ||
			terminal_key == MWKEY_LALT || terminal_key == MWKEY_RALT ||
			terminal_key == MWKEY_LCTRL || terminal_key == MWKEY_RCTRL)
		{

		}
		else if (((modifier & OZTERM_KEYM_LEFTSHIFT) || (modifier & OZTERM_KEYM_RIGHTSHIFT)) &&
			(terminal_key == OZTERM_KEY_PAGEUP || terminal_key == OZTERM_KEY_PAGEDOWN))
		{
			int16_t offset = terminal_key == OZTERM_KEY_PAGEUP ? 1 : -1;

			ozterm_scroll(g_terminal->term, ozterm_get_scroll(g_terminal->term) + offset);
		}
		else
		{
			int16_t scroll = ozterm_get_scroll(g_terminal->term);
			if (scroll != 0)
			{
				ozterm_scroll(g_terminal->term, 0);
			}
			ozterm_send_key(g_terminal->term, modifier, (uint8_t)terminal_key);
		}
	}
}

static int get_scrollbar_height(Ozterm* term)
{
    int16_t row_count = ozterm_get_row_count(term);

    int win_height = row_count * g_font_info.height;
    int total_lines = ozterm_get_scroll_count(term) + row_count;
    int visible_lines = row_count;

    // Calculate scrollbar height and position
    float visible_ratio = (float)visible_lines / total_lines;
    int bar_height = (int)(visible_ratio * win_height);
    if (bar_height < 10) bar_height = 10; // minimum size

    return bar_height;
}

static void draw_scrollbar(Ozterm* term)
{
    int16_t scrollback_count = ozterm_get_scroll_count(term);

    // Only show scrollbar if scrollback exists
    if (scrollback_count > 0)
    {
        int scroll_offset = ozterm_get_scroll(term);

        int win_height = ozterm_get_row_count(term) * g_font_info.height;
        int bar_x = ozterm_get_column_count(term) * g_font_info.maxwidth - SCROLLBAR_WIDTH - SCROLLBAR_MARGIN;

        int bar_height = get_scrollbar_height(term);

        int max_offset = scrollback_count;
        if (max_offset == 0) max_offset = 1; // avoid divide-by-zero

        float scroll_ratio = (float)scroll_offset / max_offset;
        int bar_y = (int)((1.0f - scroll_ratio) * (win_height - bar_height));

		GrFillRect(g_window_id, g_graphics_context, bar_x, bar_y, SCROLLBAR_WIDTH, bar_height);
    }
}

static void write_to_master(Ozterm* term, const uint8_t* data, int32_t size)
{
    if (g_master_fd >= 0)
    {
        write(g_master_fd, data, size);
    }
}

static void terminal_refresh(Ozterm* term)
{
    window_refresh(term);
}

static void terminal_set_character(Ozterm* term, int16_t row, int16_t column, OztermCell * cell)
{
	char text[2];
	text[0] = cell->character;
	text[1] = 0;
	window_put_text(row, column, text);
}

static void terminal_move_cursor(Ozterm* term, int16_t old_row, int16_t old_column, int16_t row, int16_t column)
{
	int16_t scroll_offset = ozterm_get_scroll(term);

    // Only draw the cursor when not scrolled
    if (scroll_offset != 0)
        return;

	char text[2];

	GrSetGCUseBackground(g_graphics_context, TRUE);

	if (old_row != row || old_column != column)
	{
		OztermCell* old_row_data = ozterm_get_row_data(term, old_row);
		OztermCell* old_cell = old_row_data + old_column;

		GrSetGCBackground(g_graphics_context, stdbackground);
		GrSetGCForeground(g_graphics_context, stdforeground);
		
		text[0] = old_cell->character;
		text[1] = 0;
		window_put_text(old_row, old_column, text);

		//old cursor pos character restored
	}

    //now draw cursor

	GrSetGCBackground(g_graphics_context, stdforeground);
	GrSetGCForeground(g_graphics_context, stdbackground);

	OztermCell* row_data = ozterm_get_row_data(term, row);
    OztermCell* cell = row_data + column;
	text[0] = cell->character;
	text[1] = 0;
	window_put_text(row, column, text);

	GrSetGCBackground(g_graphics_context, stdbackground);
	GrSetGCForeground(g_graphics_context, stdforeground);
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
    int winw = COLS * g_font_info.maxwidth;
    int winh = ROWS * g_font_info.height;

    g_window_id = GrNewWindow(GR_ROOT_WINDOW_ID, 10, 10, winw, winh, 0, stdbackground, stdforeground);

    props.flags = GR_WM_FLAGS_TITLE;
    props.title = TITLE;
    GrSetWMProperties(g_window_id, &props);

    GrSelectEvents(g_window_id, GR_EVENT_MASK_BUTTON_DOWN | GR_EVENT_MASK_EXPOSURE |
		   GR_EVENT_MASK_KEY_DOWN | 
		   GR_EVENT_MASK_FOCUS_IN | GR_EVENT_MASK_FOCUS_OUT |
		   GR_EVENT_MASK_UPDATE | GR_EVENT_MASK_CLOSE_REQ);
    GrMapWindow(g_window_id);

    g_graphics_context = GrNewGC();
    GrSetGCFont(g_graphics_context, g_font);

    GrSetGCForeground(g_graphics_context, stdforeground);
    GrSetGCBackground(g_graphics_context, stdbackground);
    GrGetWindowInfo(g_window_id,&g_window_info);
    GrGetGCInfo(g_graphics_context, &g_gc_info);

    /*
     * create a pty
     */
    g_master_fd = term_init();
	GrRegisterInput(g_master_fd);

	Terminal* terminal = malloc(sizeof(Terminal));
    g_terminal = terminal;
    memset(terminal, 0, sizeof(Terminal));

    Ozterm * term = ozterm_create(ROWS, COLS);
    ozterm_set_write_to_master_callback(term, write_to_master);
    ozterm_set_render_callbacks(term, terminal_refresh, terminal_set_character, terminal_move_cursor);
    ozterm_set_custom_data(term, terminal);
    terminal->term = term;

	char buf[8192];
	
	GR_EVENT wevent;
	GR_EVENT_KEYSTROKE *kp = NULL;
	GR_EVENT_BUTTON *button_event = NULL;
	
	while (1)
	{
		GrGetNextEvent(&wevent);

		switch(wevent.type)
		{
		case GR_EVENT_TYPE_CLOSE_REQ:
			GrClose();
			exit(0);
			break;
		case GR_EVENT_TYPE_EXPOSURE:
			window_refresh(g_terminal->term);
			break;

		case GR_EVENT_TYPE_KEY_DOWN:
			kp = (GR_EVENT_KEYSTROKE *)&wevent;
			handle_key_press(kp);
			break;
		case GR_EVENT_TYPE_BUTTON_DOWN:
			button_event = (GR_EVENT_BUTTON*)&wevent;
			if (button_event->wid == g_window_id)
			{
				//GrSetFocus(g_window_id);
				//GrRaiseWindow(g_window_id);
			}
			break;
		case GR_EVENT_TYPE_FDINPUT:
		{
			int len = read(g_master_fd, buf, sizeof(buf));
			if (len >= 0)
			{
				ozterm_have_read_from_master(term, (uint8_t*)buf, len);
			}
		}
		break;
		}

	}
    return 0;
}