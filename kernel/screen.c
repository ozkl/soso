#include "screen.h"
#include "common.h"
#include "serial.h"

#define SCREEN_LINE_COUNT 25
#define SCREEN_COLUMN_COUNT 80

static unsigned char *videoStart = (unsigned char*)0xB8000;

static uint16 gCurrentLine = 0;
static uint16 gCurrentColumn = 0;
static uint8 gColor = 0x0A;

void Screen_CopyFrom(uint8* buffer)
{
    memcpy(videoStart, buffer, SCREEN_LINE_COUNT * SCREEN_COLUMN_COUNT * 2);
}

void Screen_CopyTo(uint8* buffer)
{
    memcpy(buffer, videoStart, SCREEN_LINE_COUNT * SCREEN_COLUMN_COUNT * 2);
}

void Screen_Print(int row, int column, const char* text)
{
    unsigned char * video = videoStart;
    
    video += (row * SCREEN_COLUMN_COUNT + column) * 2;
    while(*text != 0)
    {
        *video++ = *text++;
        *video++ = gColor;
    }
}

//One line
void Screen_ScrollUp()
{
    unsigned char * videoLine = videoStart;
	unsigned char * videoLineNext = videoStart;
	int line = 0;
	int column = 0;
    
    for (line = 0; line < SCREEN_LINE_COUNT - 1; ++line)
    {
		for (column = 0; column < SCREEN_COLUMN_COUNT; ++column)
		{
			videoLine = videoStart + (line * SCREEN_COLUMN_COUNT + column) * 2;
			videoLineNext = videoStart + ((line + 1) * SCREEN_COLUMN_COUNT + column) * 2;

			videoLine[0] = videoLineNext[0];
			videoLine[1] = videoLineNext[1];
        }
    }
    
    //Last line should be empty.
    unsigned char * lastLine = videoStart + ((SCREEN_LINE_COUNT - 1) * SCREEN_COLUMN_COUNT) * 2;
    for (int i = 0; i < SCREEN_COLUMN_COUNT * 2; i += 2)
    {
        lastLine[i] = 0;
        lastLine[i + 1] = gColor;
    }
}

void Screen_SetActiveColor(uint8 color)
{
    gColor = color;
}

void Screen_ApplyColor(uint8 color)
{
    gColor = color;

    unsigned char * video = videoStart;
    int i = 0;

    for (i = 0; i < SCREEN_LINE_COUNT * SCREEN_COLUMN_COUNT; ++i)
    {
        video++;
        *video++ = gColor;
    }
}

void Screen_Clear()
{
	unsigned char * video = videoStart;
	int i = 0;
    
    for (i = 0; i < SCREEN_LINE_COUNT * SCREEN_COLUMN_COUNT; ++i)
    {
        *video++ = 0;
        *video++ = gColor;
    }
    
    gCurrentLine = 0;
    gCurrentColumn = 0;
}

void Screen_PutChar(char c)
{
	unsigned char * video = videoStart;

    //writeSerial(c);

	if ('\n' == c || '\r' == c)
	{
		++gCurrentLine;
		gCurrentColumn = 0;

        if (gCurrentLine >= SCREEN_LINE_COUNT - 0)
        {
            --gCurrentLine;
            Screen_ScrollUp();
        }

        Screen_MoveCursor(gCurrentLine, gCurrentColumn);
        return;
	}
    else if ('\b' == c)
    {
        if (gCurrentColumn > 0)
        {
            --gCurrentColumn;
            c = '\0';
            video = videoStart + (gCurrentLine * SCREEN_COLUMN_COUNT + gCurrentColumn) * 2;
            video[0] = c;
            video[1] = gColor;
            Screen_MoveCursor(gCurrentLine, gCurrentColumn);
            return;
        }
        else if (gCurrentColumn == 0)
        {
            if (gCurrentLine > 0)
            {
                --gCurrentLine;
                gCurrentColumn = SCREEN_COLUMN_COUNT - 1;
                c = '\0';
                video = videoStart + (gCurrentLine * SCREEN_COLUMN_COUNT + gCurrentColumn) * 2;
                video[0] = c;
                video[1] = gColor;
                Screen_MoveCursor(gCurrentLine, gCurrentColumn);
                return;
            }
        }
    }

    if (gCurrentColumn >= SCREEN_COLUMN_COUNT)
	{
		++gCurrentLine;
		gCurrentColumn = 0;
	}
    
    if (gCurrentLine >= SCREEN_LINE_COUNT - 0)
	{
		--gCurrentLine;
		Screen_ScrollUp();
	}
		
    video += (gCurrentLine * SCREEN_COLUMN_COUNT + gCurrentColumn) * 2;
    
	video[0] = c;
    video[1] = gColor;
	
	++gCurrentColumn;
	
	Screen_MoveCursor(gCurrentLine, gCurrentColumn);
}

 void Screen_PrintF(const char *format, ...)
 {
   char **arg = (char **) &format;
   char c;
   char buf[20];
 
   //arg++;
   __builtin_va_list vl;
   __builtin_va_start(vl, format);
 
   while ((c = *format++) != 0)
	 {
	   if (c != '%')
		 Screen_PutChar(c);
	   else
		 {
		   char *p;
 
		   c = *format++;
		   switch (c)
			 {
			 case 'x':
				buf[0] = '0';
				buf[1] = 'x';
                //itoa (buf + 2, c, *((int *) arg++));
                itoa (buf + 2, c, __builtin_va_arg(vl, int));
			    p = buf;
			    goto string;
			    break;
			 case 'd':
			 case 'u':
               //itoa (buf, c, *((int *) arg++));
               itoa (buf, c, __builtin_va_arg(vl, int));
			   p = buf;
			   goto string;
			   break;
 
			 case 's':
               //p = *arg++;
               p = __builtin_va_arg(vl, char*);
			   if (! p)
				 p = "(null)";
 
			 string:
			   while (*p)
				 Screen_PutChar(*p++);
			   break;
 
			 default:
               //Screen_PutChar(*((int *) arg++));
               Screen_PutChar(__builtin_va_arg(vl, int));
			   break;
			 }
		 }
	 }
  __builtin_va_end(vl);
 }
 
void Screen_MoveCursor(uint16 line, uint16 column)
{
   // The screen is 80 characters wide...
   uint16 cursorLocation = line * SCREEN_COLUMN_COUNT + column;
   outb(0x3D4, 14);                  // Tell the VGA board we are setting the high cursor byte.
   outb(0x3D5, cursorLocation >> 8); // Send the high cursor byte.
   outb(0x3D4, 15);                  // Tell the VGA board we are setting the low cursor byte.
   outb(0x3D5, cursorLocation);      // Send the low cursor byte.

   gCurrentColumn = column;
   gCurrentLine = line;
}

void Screen_SetCursorVisible(BOOL visible)
{
    uint8 cursor = inb(0x3d5);

    if (visible)
    {
        cursor &= ~0x20;//5th bit cleared when cursor visible
    }
    else
    {
        cursor |= 0x20;//5th bit set when cursor invisible
    }
    outb(0x3D5, cursor);
}

 void Screen_GetCursor(uint16* line, uint16* column)
 {
     *line = gCurrentLine;
     *column = gCurrentColumn;
 }

void Screen_PrintInterruptsEnabled()
{
    if (isInterruptsEnabled())
    {
        Screen_PrintF("Interrupts: enabled\n");
    }
    else
    {
        Screen_PrintF("Interrupts: disabled\n");
    }
}
