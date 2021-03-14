/*   
 * taken from nxterm - terminal emulator for Nano-X
 * ported to soso
 */

#define GR_COLOR_WHITESMOKE MWRGB(245,245,245)
#define GR_COLOR_GAINSBORO MWRGB(220,220,220)
#define GR_COLOR_ANTIQUEWHITE MWRGB(250,235,215)
#define GR_COLOR_BLANCHEDALMOND MWRGB(255,235,205)
#define GR_COLOR_LAVENDER MWRGB(230,230,250)
#define GR_COLOR_WHITE MWRGB(255,255,255)

#define _XOPEN_SOURCE 600
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MWINCLUDECOLORS
#include "nano-X.h"

#include <soso.h>

static char termtype_string[] = "TERM=ngterm";
static char termcap_string[1024] =
"TERMCAP=ngterm|nano-X vt52 terminal:\
al=\\EL:am:bc=\\ED:bl=^G:bs:cd=\\EJ:\
ce=\\EK:cl=\\EE:cm=\\EY%+ %+ :cr=^M:dl=\\EM:do=\\EB:ho=\\EH:is=\\Ev\\Eq:\
l0=F10:le=\\ED:ms:nd=\\EC:rc=\\Ek:rs=\\Ev\\Eq\\EE:sc=\\Ej:sr=\\EI:\
ti=\\Ev\\Ee\\EG:up=\\EA:ve=\\Ee:vi=\\Ef:so=\\Ep:se=\\Eq:mb=\\Ei:md=\\Eg:\
mr=\\Ep:me=\\EG\\Eq:te=\\EG:us=\\Ei:ue=\\EG:\
kb=^H:kl=\\ED:kr=\\EC:ku=\\EA:kd=\\EB:kI=\\EI:kh=\\EE:kP=\\Ea:kN=\\Eb:\
k0=\\EY:k1=\\EP:k2=\\EQ:k3=\\ER:k4=\\ES:k5=\\ET:k6=\\EU:k7=\\EV:k8=\\EW:\
k9=\\EX:s0=\\Ey:";

#include "uni_std.h"
#define UNIX 1
#if UNIX
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
//#include <utmp.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#define NSIG 	_NSIG
#define UNIX98	1		/* use new-style /dev/ptmx, /dev/pts/0*/
#endif

#define stdforeground BLACK
//#define stdbackground GR_COLOR_GAINSBORO //LTGRAY
//#define stdbackground BLUE //LTGRAY
#define stdbackground LTGRAY
#define stdcol 80
#define stdrow 25

#define TITLE		"term"
#define	SMALLBUFFER stdcol //80
#define	LARGEBUFFER 10240 //keyboard

#define debug_screen 0
#define debug_kbd 0

/*
 * globals
 */
GR_WINDOW_ID	w1;		/* id for window */
GR_GC_ID		gc1;	/* graphics context */
GR_FONT_ID   	regFont;
/*GR_FONT_ID boldFont;*/
GR_SCREEN_INFO	si;	/* screen info */
GR_FONT_INFO    fi;	/* Font Info */

#define fonh fi.height
#define fonw fi.maxwidth

GR_WINDOW_INFO  wi;
GR_GC_INFO  gi;
GR_BOOL		havefocus = GR_FALSE;
pid_t 		pid;
short 		winw, winh;
int 		pipeh;
short 		cblink = 0, visualbell = 0, debug = 0;
int 		fgcolor[12] = { 0,1,2,3,4,5,6,7,8,9,11 };
int 		bgcolor[12] = { 0,1,2,3,4,5,6,7,8,9,11 };
int 		scrolledFlag; 	/* set when screen has been scrolled up */
int 		isMaximized = 0;

char prog_to_start[128];
int scrolltop;
int scrollbottom;
int ReverseMode=0;
int semicolonflag = 0;
int nobracket = 0;
int roundbracket = 0;
int savex;
int savey;

/* the wterm terminal code, almost like VT52 */
short	termtype = 0; // 0=ANSI
short	bgmode, escstate, curx, cury, curon, curvis;
short	savx, savy, wrap, style;
short	col, row, colmask = 0x7f, rowmask = 0x7f;
short	sbufcnt = 0;
short	sbufx, sbufy;
char    lineBuffer[SMALLBUFFER+1];
char	*sbuf = lineBuffer;

void sigchild(int signo);
int term_init(void);
void sflush(void);
void lineRedraw(void);
void sadd(char c);
void show_cursor(void);
void draw_cursor(void);
void hide_cursor(void);
void vscrollup(int lines);
void vscrolldown(int lines);
void esc5(unsigned char c);
void esc4(unsigned char c);
void esc3(unsigned char c);
void esc2(unsigned char c);
void esc1(unsigned char c);
void esc0(unsigned char c);
void esc100(unsigned char c); //ANSI codes
void printc(unsigned char c);
void init(void);
void term(void);
void usage(char *s);
int do_special_key(unsigned char *buffer, int key, int modifiers);
int do_special_key_ansi(unsigned char *buffer, int key, int modifiers);
void pos_xaxis(int c);	/* cursor position x axis for ansi */
void pos_yaxis(int c);	/* cursor position x axis for ansi */
void rendition(int escvalue);

/* **************************************************************************/

void sflush(void)
{
	if (sbufcnt) {
		GrText(w1,gc1, sbufx*fonw, sbufy*fonh, sbuf, sbufcnt, GR_TFTOP);
		sbufcnt = 0;
	}
}

void lineRedraw(void)
{
	GrSetGCForeground(gc1,gi.background);
	GrFillRect(w1, gc1, curx*fonw, cury*fonh, (col-curx)*fonw, fonh);
	GrSetGCForeground(gc1,gi.foreground);

	if (sbufcnt) {
		sbuf[sbufcnt] = 0;
		GrText(w1,gc1, sbufx*fonw, sbufy*fonh, sbuf, sbufcnt, GR_TFTOP);
	}
}

void sadd(char c)
{
    if (sbufcnt == SMALLBUFFER)
		sflush ();

    if (!sbufcnt) { 
		sbufx = curx; 
		sbufy = cury; 
    } 
    sbuf[sbufcnt++] = c;
}

void show_cursor(void)
{
	GrSetGCMode(gc1,GR_MODE_XOR);
	GrSetGCForeground(gc1, WHITE);
	GrFillRect(w1, gc1, curx*fonw, cury*fonh+1, fonw, fonh-1);
	GrSetGCForeground(gc1, gi.foreground);
	GrSetGCMode(gc1,GR_MODE_COPY);
}


void draw_cursor (void)
{
	if(curon)
		if(!curvis) {
			curvis = 1;
			show_cursor();
			}
}
void hide_cursor (void)
{
	if(curvis) {
		curvis = 0;
		show_cursor();
	}
}


void vscrollup(int lines)
{
    hide_cursor();
    GrCopyArea(w1,gc1, 0, (scrolltop-1)*fonh, winw, (scrollbottom-(scrolltop-1)-lines)*fonh, w1, 0, (scrolltop-1+lines)*fonh, MWROP_COPY);
    GrSetGCForeground(gc1,gi.background);    
    GrFillRect(w1, gc1, 0, (scrollbottom-lines)*fonh, winw, lines*fonh);
    GrSetGCForeground(gc1,gi.foreground);    

}

void vscrolldown(int lines)
{
    hide_cursor();
    GrCopyArea(w1,gc1, 0, (scrolltop+lines)*fonh, winw, (scrollbottom-scrolltop-lines)*fonh, w1, 0, scrolltop*fonh, MWROP_COPY);
    GrSetGCForeground(gc1,gi.background);    
    GrFillRect(w1, gc1, 0, scrolltop*fonh, winw, lines*fonh);
    GrSetGCForeground(gc1,gi.foreground);    
}

void esc5(unsigned char c)	/* setting background color */
{
    GrSetGCBackground(gc1, c);
    GrGetGCInfo(gc1,&gi);
    escstate = 0;
}

void esc4(unsigned char c)	/* setting foreground color */
{
    GrSetGCForeground(gc1,c);
    GrGetGCInfo(gc1,&gi);
    escstate = 0;
}

void esc3(unsigned char c)	/* cursor position x axis */
{
    curx = (c - 32) & colmask;
    if (curx >= col)
	curx = col - 1;
    else if (curx < 0)
	curx = 0;
    escstate = 0;
}


void esc2(unsigned char c)	/* cursor position y axis */
{
	cury = (c - 32) & rowmask;
	if (cury >= row)
		cury = row - 1;
	else if (cury < 0)
		cury = 0;
	escstate = 3;
}


void esc1(unsigned char c)	/* various control codes */
{

    escstate = 0;

    /* detect ANSI / VT100 codes */
    if (c=='['){
          escstate = 10;  
          return; 
    }
    if (c=='('){
          escstate = 10;  
	  roundbracket=1;
          return; 
    }
    if (termtype==0){
	//no bracket code - just ESC+letter
	//so read no further char just do esc100 and terminate
	//ESC state to read new sequence or unescaped chars.
	  nobracket = 1;
	  esc100(c);
          escstate = 0;  
          return; 
    }

//now vt52 codes
    switch(c) {
    case 'A':/* cursor up */
		hide_cursor();
		if ((cury -= 1) < 0)
    		cury = 0;
		break;

    case 'B':/* cursor down */
		hide_cursor();
		if ((cury += 1) >= row)
	    	cury = row - 1;
		break;

    case 'C':/* cursor right */
		hide_cursor();
		if ((curx += 1) >= col)
	    	curx = col - 1;
		break;

    case 'D':/* cursor left */
		hide_cursor();
		if ((curx -= 1) < 0)
	    	curx = 0;
		break;

    case 'E':/* clear screen & home */
		GrClearWindow(w1, 0);
		curx = 0;
		cury = 0;
	break;

    case 'H':/* cursor home */
		curx = 0;
		cury = 0;
		break;

    case 'I':/* reverse index */
		scrolledFlag = 1;
		if ((cury -= 1) < 0) {
	    	cury = 0;
	    	vscrollup(1);
		}
		break;

    case 'J':/* erase to end of page */
 		if (cury < row-1) {
	    	GrSetGCForeground(gc1,gi.background);
	    	GrFillRect(w1,gc1, 0,(cury+1)*fonh, winw, (row-1-cury)*fonh);
	    	GrSetGCForeground(gc1,gi.foreground);
		}
		GrSetGCForeground(gc1,gi.background);
 		GrFillRect(w1, gc1, curx*fonw, cury*fonh, (col-curx)*fonw, fonh);
		GrSetGCForeground(gc1,gi.foreground);
		break;

    case 'K':/* erase to end of line */
		GrSetGCForeground(gc1,gi.background);
		GrFillRect(w1, gc1, curx*fonw, cury*fonh, (col-curx)*fonw, fonh);
		GrSetGCForeground(gc1,gi.foreground);
		break;

    case 'L':/* insert line */
 		if (cury < row-1) 
		{
	    	vscrollup(1);
		}
 		curx = 0;
		break;

    case 'M':/* delete line */
 		if (cury < row-1) 
	    	vscrollup(1);
 		curx = 0;
		break;

    case 'Y':/* position cursor */
		escstate = 2;
		break;

    case 'b':/* set foreground color */
		escstate = 4;
		break;

    case 'c':/* set background color */
		escstate = 5;
		break;

    case 'd':/* erase beginning of display */
		/* 	w_setmode(win, bgmode); */
 		if (cury > 0) {
			GrSetGCForeground(gc1,gi.background);
			GrFillRect(w1,gc1, 0, 0, winw, cury*fonh);
			GrSetGCForeground(gc1,gi.foreground);
		}
 		if (curx > 0) {
	    	GrSetGCForeground(gc1,gi.background);
	    	GrFillRect(w1,gc1, 0, cury*fonh, curx*fonw, fonh);
	    	GrSetGCForeground(gc1,gi.foreground);
		}
		break;

    case 'e':/* enable cursor */
		curon = 1;
		break;

    case 'f':/* disable cursor */
		curon = 0;
		break;

    case 'j':/* save cursor position */
		savx = curx;
		savy = cury;
		break;

    case 'k':/* restore cursor position */
		curx = savx;
		cury = savy;
		break;

    case 'l':/* erase entire line */
		GrSetGCForeground(gc1,gi.background);
		GrFillRect(w1,gc1, 0, cury*fonh, winw, fonh);
		GrSetGCForeground(gc1,gi.foreground);
		curx = 0;
		break;

    case 'o':/* erase beginning of line */
		if (curx > 0) {
	    	GrSetGCForeground(gc1,gi.background);
	    	GrFillRect(w1,gc1,0, cury*fonh, curx*fonw, fonh);
	    	GrSetGCForeground(gc1,gi.foreground);
		}
		break;

    case 'p':/* enter reverse video mode */
		if(!ReverseMode) {
	    	GrSetGCForeground(gc1,gi.background);
	    	GrSetGCBackground(gc1,gi.foreground);
 	    	ReverseMode=1; 
    	}
		break;

    case 'q':/* exit reverse video mode */
		if(ReverseMode) {
	    	GrSetGCForeground(gc1,gi.foreground);
	    	GrSetGCBackground(gc1,gi.background);
 	    	ReverseMode=0;
		}
		break;

    case 'v':/* enable wrap at end of line */
		wrap = 1;
		break;

    case 'w':/* disable wrap at end of line */
		wrap = 0;
		break;

/* and these are the extentions not in VT52 */
    case 'G': /* clear all attributes */
		break;

    case 'g': /* enter bold mode */
		/*GrSetGCFont(gc1, boldFont); */
		break;

    case 'h': /* exit bold mode */
		/*	GrSetGCFont(gc1, regFont); */
		break;

    case 'i': /* enter underline mode */
		break;

	/* j, k and l are already used */
    case 'm': /* exit underline mode */
		break;

/* these ones aren't yet on the termcap entries */
    case 'n': /* enter italic mode */
		break;

	/* o, p and q are already used */
    case 'r': /* exit italic mode */
	break;

    case 's': /* enter light mode */
		break;

    case 't': /* exit ligth mode */
		break;

    default: /* unknown escape sequence */
#if debug_screen
	GrError("default:>%c<,",c);
#endif

		break;
    }
}


void pos_xaxis(int c)	/* cursor position x axis for ansi */
{
    curx = c & colmask;
    if (curx >= col)
	curx = col - 1;
    else if (curx < 0)
	curx = 0;
}


void pos_yaxis(int c)	/* cursor position y axis for ansi */
{
	cury = c & rowmask;
/*	if (cury >= row)
		cury = row - 1;
	else if (cury < 0)
		cury = 0;
*/
	if (cury >= scrollbottom)
		cury = scrollbottom - 1;
	else if (cury < scrolltop)
		cury = scrolltop-1;
}


void rendition(int escvalue) {

		if (escvalue==0) { //reset to default
    			GrSetGCForeground(gc1, stdforeground);
    			GrSetGCBackground(gc1, stdbackground);
	 	    	ReverseMode=0; 
		} else if (escvalue==7) { //inverse 
			if(!ReverseMode) {
		    	GrSetGCForeground(gc1,gi.background);
		    	GrSetGCBackground(gc1,gi.foreground);
 	    		ReverseMode=1; 
	    		}
		} else if (escvalue==27) { //inverse off
			if(ReverseMode) {
		    	GrSetGCForeground(gc1,gi.foreground);
		    	GrSetGCBackground(gc1,gi.background);
 	    		ReverseMode=0;
			}
		} else if ((escvalue>29) && (escvalue<38)){
    		switch(escvalue) {
		    case 30:
    			GrSetGCForeground(gc1, BLACK);
    			break;
		    case 31:
    			GrSetGCForeground(gc1, RED);
    			break;
		    case 32:
    			GrSetGCForeground(gc1, GREEN);
    			break;
		    case 33:
    			GrSetGCForeground(gc1, BROWN);
    			break;
		    case 34:
    			GrSetGCForeground(gc1, BLUE);
    			break;
		    case 35:
    			GrSetGCForeground(gc1, MAGENTA);
    			break;
		    case 36:
    			GrSetGCForeground(gc1, CYAN);
    			break;
		    case 37:
    			GrSetGCForeground(gc1, WHITE);
    			break;
		    case 39:
    			GrSetGCForeground(gc1, stdforeground); //default color
    			break;
    		}
		} else if ((escvalue>39) && (escvalue<49)){
    		switch(escvalue) {
		    case 40:
    			GrSetGCBackground(gc1, BLACK);
    			break;
		    case 41:
    			GrSetGCBackground(gc1, RED);
    			break;
		    case 42:
    			GrSetGCBackground(gc1, GREEN);
    			break;
		    case 43:
    			GrSetGCBackground(gc1, BROWN);
    			break;
		    case 44:
    			GrSetGCBackground(gc1, BLUE);
    			break;
		    case 45:
    			GrSetGCBackground(gc1, MAGENTA);
    			break;
		    case 46:
    			GrSetGCBackground(gc1, CYAN);
    			break;
		    case 47:
    			GrSetGCBackground(gc1, WHITE);
    			break;
		    case 49:
    			GrSetGCBackground(gc1, stdbackground); //default color
    			break;
    		}
		} //if escvalue

} //rendition

void esc100(unsigned char c)	/* various ANSI control codes */
{
//leave escstate=10 till done. This states gets this function called.

static int escvalue1,escvalue2,escvalue3;
static char valuebuffer[3];
valuebuffer[2]='\0';

if (c == '?') return; //skip question mark for now, e.g. ESC[?7h for wrap on

if (nobracket==1){
	//fall through
}else if (roundbracket == 1) { //just remove ESC(B for set US ASCII
    	//fall through
}
else if ( (c > 0x2F) && (c<':') ) // is it a number?
{

	if (valuebuffer[0] != '\0' )
		{ valuebuffer[1]=c; 
	} else {
		valuebuffer[0]=c;
	}
	return; 
}
else if (c == ';')
{
	if (semicolonflag==0) {
		escvalue1=atoi(valuebuffer);
		semicolonflag++;
		valuebuffer[0]='\0';
		valuebuffer[1]='\0';
		return; 
	} else if (semicolonflag==1) {
		escvalue2=atoi(valuebuffer);
		semicolonflag++;
		valuebuffer[0]='\0';
		valuebuffer[1]='\0';
		return; 
	}
}
else if (((c > '@')&&(c<'[')) || ((c>0x60)&&(c<'{'))) //is it a letter?
{
	if (semicolonflag==0) {
		escvalue1=atoi(valuebuffer);
		escvalue2=0;
		escvalue3=0;
	}
	else if (semicolonflag==1) {
		escvalue2=atoi(valuebuffer);
		escvalue3=0;
	}
	else if (semicolonflag==2) {
		escvalue3=atoi(valuebuffer);
	}
	escstate=0;
	valuebuffer[0]='\0';
	valuebuffer[1]='\0';
} else { //unknown
	return;
}
//fall through now if letter received

//now interpret the ESC sequence
/*
the cursor positions: cury,curx are zero based, so 0,0 is home position
also if command asks to position to 5 this has to be 4
y is the row/line position, x is the column position
fonh = fi.height = height of character or line in pixel
fonw = fi.maxwidth = width of character in pixel
winw = width of line in pixel
winh = height of screen in pixel
col = number of columns for current screen width
row = number of lines for current screen height
scrolltop, scrollbottom = upper and lower scroll region limit in lines/rows
*/
    //GrError("\n\nC:%c,val1:%d,val2:%d,val3:%d,scflag:%d,curx:%d,cury:%d,nobracket:%d\n",c,escvalue1,escvalue2,escvalue3,semicolonflag,curx,cury,nobracket);

    if (nobracket==1){
    	if (c=='8'){ //Restore cursor
    		//HOME will reduce by one below, so add here!
    		escvalue1=savey+1;
    		escvalue2=savex+1;
    		c='H'; //position cursor
    	} else if (c=='7'){ //save cursor
    		  savex=curx;
    		  savey=cury;
    		  c='!'; //done, so invalid code
    	} else if (c=='M'){ //reverse index, same as cursor up but scroll display at top
		if (cury <= scrolltop){
    		  	escvalue1=1;
    		  	c='L'; //insert line
    		  	//cury--;
	 		//vscrolldown(1);
		  	//c='!'; //invalid code
 		} else {
			cury--;
		  	pos_yaxis(cury); 
		  	c='!'; //invalid code
		}

    	} else if (c=='D'){ //index, same as cursor down but scroll display at bottom
		if ((cury+1) > scrollbottom){
    		  	escvalue1=1;
    		  	c='M'; //delete line
	 		//vscrollup(1);
		  	//c='!'; //invalid code
 		} else {
			cury++;
		  	pos_yaxis(cury); 
		  	c='!'; //invalid code
		}

	} //c==8
    } //nobracket
    nobracket=0; //always reset
    
    if (roundbracket==1){ //remove ESC(B for set to US ASCII
      c=0;
      roundbracket=0;
    }

    switch(c) {
    case 'A':/* cursor up */
		if (escvalue1==0) escvalue1=1;
		hide_cursor();
		if ((cury -= escvalue1) < 0)
    		//cury = 0;
    		cury = scrolltop-1; //cury zero based
		break;

    case 'B':/* cursor down */
		if (escvalue1==0) escvalue1=1;
		hide_cursor();
		if ((cury += escvalue1) >= row)
	    	//cury = row - 1;
	    	cury = scrollbottom-1; //cury zero based 
		break;

    case 'C':/* cursor right */
		if (escvalue1==0) escvalue1=1;
		hide_cursor();
		if ((curx += escvalue1) >= col)
	    	curx = col - 1;
		break;

    case 'D':/* cursor left */
		if (escvalue1==0) escvalue1=1;
		hide_cursor();
		if ((curx -= escvalue1) < 0)
	    	curx = 0;
		break;

    case 'J':
    	if (escvalue1==0) { //erase from current cursor to end of page/scrollbottom
 		if (cury < scrollbottom-1) { //erase area below current line
	    	GrSetGCForeground(gc1,gi.background);
	    	GrFillRect(w1,gc1, 0,(cury+1)*fonh, winw, (scrollbottom-1-cury)*fonh);
	    	GrSetGCForeground(gc1,gi.foreground);
		} //erase from cursor to end of line
		GrSetGCForeground(gc1,gi.background);
 		GrFillRect(w1, gc1, curx*fonw, cury*fonh, (col-curx)*fonw, fonh);
		GrSetGCForeground(gc1,gi.foreground);
		break;
    	} else if (escvalue1==1) { //erase from home/scrolltop to cursor
 		if (cury < scrollbottom-1) { //erase area from top to line above current line
	    	GrSetGCForeground(gc1,gi.background);
	    	GrFillRect(w1,gc1, 0, scrolltop, winw, (cury+1)*fonh); 
	    	GrSetGCForeground(gc1,gi.foreground);
		} //erase from beginning of line to cursor position
		GrSetGCForeground(gc1,gi.background);
 		GrFillRect(w1, gc1, 0, cury*fonh, curx*fonw, fonh); 
		GrSetGCForeground(gc1,gi.foreground);
		break;
   	} else if (escvalue1==2) { //erase entire page - leave cursor untouched
		//GrClearWindow(w1, 0);
		//erase just the scrolling area
		GrSetGCForeground(gc1,gi.background);
	    	GrFillRect(w1,gc1, 0, scrolltop, winw, (scrollbottom)*fonh); 
	    	GrSetGCForeground(gc1,gi.foreground);
		break;
    	}

    case 'K':/* erase to end of line */
    	if (escvalue1==0) { //erase from current cursor to end of line
		GrSetGCForeground(gc1,gi.background);
		GrFillRect(w1, gc1, curx*fonw, cury*fonh, (col-curx)*fonw, fonh);
		GrSetGCForeground(gc1,gi.foreground);
		break;
    	} else if (escvalue1==1) { //erase from beginning of line to cursor
		GrSetGCForeground(gc1,gi.background);
		GrFillRect(w1, gc1, 0, cury*fonh, curx*fonw, fonh);
		GrSetGCForeground(gc1,gi.foreground);
		break;
   	} else if (escvalue1==2) { //erase entire line - leave cursor untouched
		GrSetGCForeground(gc1,gi.background);
		GrFillRect(w1, gc1, 0, cury*fonh, winw, fonh);
		GrSetGCForeground(gc1,gi.foreground);
		break;
	}

    case 'P':/* erase number of characters after and including the cursor and move remaining to this position */
		if (escvalue1==0) escvalue1=1;
		GrSetGCForeground(gc1,gi.background);
		//copy remaining chars on line to cursor position
		GrCopyArea(w1,gc1, curx*fonw, cury*fonh, (col-curx-escvalue1)*fonw, fonh, w1, (curx+escvalue1)*fonw, cury*fonh, MWROP_COPY);
		//clear space at end of line
		GrFillRect(w1, gc1, (col-escvalue1)*fonw, cury*fonh, (escvalue1)*fonw, fonh);
		GrSetGCForeground(gc1,gi.foreground);
		break;


    case 'L':/* insert lines */
		if (escvalue1==0) escvalue1=1;

    		hide_cursor();
    		//copy from cursor the number of lines down
		GrCopyArea(w1,gc1, 0, (cury+escvalue1)*fonh, winw, (scrollbottom-cury-escvalue1)*fonh, w1, 0, cury*fonh, MWROP_COPY);
                //clear number of lines starting at cursor position
    		GrSetGCForeground(gc1,gi.background);
    		GrFillRect(w1, gc1, 0, cury*fonh, winw, escvalue1*fonh);
    		GrSetGCForeground(gc1,gi.foreground);
		break;

    case 'M':/* delete lines */

		if (escvalue1==0) escvalue1=1; 
		GrCopyArea(w1,gc1, 0, cury*fonh, winw, (scrollbottom-cury-escvalue1)*fonh, w1, 0, (cury+escvalue1)*fonh, MWROP_COPY);

                //clear number of lines starting from scrollbottom up
    		GrSetGCForeground(gc1,gi.background);
    		GrFillRect(w1, gc1, 0, (scrollbottom-escvalue1)*fonh, winw, escvalue1*fonh);
    		GrSetGCForeground(gc1,gi.foreground);
		break;

    case 'S':/* scroll page up number of lines */
		if (escvalue1==0) escvalue1=1;
 		vscrollup(escvalue1);
		break;

    case 'T':/* scroll page down number of lines */
		if (escvalue1==0) escvalue1=1;
 		vscrolldown(escvalue1);
		break;

    case 'H':/* position cursor */
    case 'f':/* position cursor */
		if (escvalue1>0) escvalue1--;
		if (escvalue2>0) escvalue2--;
		pos_yaxis(escvalue1);
		pos_xaxis(escvalue2);
		break;

    case 'E':/* lines down, col=1 */
		if (escvalue1==0) escvalue1=1;
		hide_cursor();
		if ((cury += escvalue1) >= scrollbottom)
	    	cury = scrollbottom - 1;
                curx = 0;
		break;

    case 'F':/* lines up, col=1 */
		if (escvalue1==0) escvalue1=1;
		hide_cursor();
		if ((cury -= escvalue1) < 0)
    		cury = 0;
                curx = 0;
		break;

    case 'd':/* position cursor to row */
		if (escvalue1>0) escvalue1--;
		pos_yaxis(escvalue1);
		break;

    case '`':/* position cursor to col */
    case 'G':/* position cursor to col */
		if (escvalue1>0) escvalue1--;
		pos_xaxis(escvalue1);
		break;

    case 'e':/* move cursor down rows */
		if (escvalue1>0) escvalue1--;
		pos_yaxis(escvalue1+cury);
		break;

    case 'a':/* move cursor right columns */
		if (escvalue1>0) escvalue1--;
		pos_xaxis(escvalue1+curx);
		break;

    case 'm':/* Set graphics rendition */
	//may be more values, do just up to three here
    //foreground colors run from 30 to 37, background from 40 to 47
		rendition(escvalue1); //always, even if 0
		if (escvalue2!=0) rendition(escvalue2);
		if (escvalue3!=0) rendition(escvalue3);

	escvalue1=0;
	escvalue2=0;
	escvalue3=0;	
	GrGetGCInfo(gc1,&gi);
	break;


    case 's':/* save cursor position */
		savx = curx;
		savy = cury;
		break;

    case 'u':/* restore cursor position */
		curx = savx;
		cury = savy;
		break;

    case 'r':/* set scrolling region */
    		if ((escvalue1>-1) && (escvalue2 <= (winh-1))){
			scrolltop = escvalue1;
			scrollbottom = escvalue2;
		}
		break;

    case 'h':/* enable private modes - e.g. wrap at end of line */
		if (escvalue1==7) wrap = 1;
		if (escvalue1==25) show_cursor();
		break;

    case 'l':/* disable private modes - e.g. wrap at end of line */
		if (escvalue1==7) wrap = 0;
		if (escvalue1==25) hide_cursor();
		break;

    default: /* unknown escape sequence */
		break;
    }
    //GrError("end-C:%c,val1:%d,val2:%d,val3:%d,scflag:%d,curx:%d,cury:%d\n",c,escvalue1,escvalue2,escvalue3,semicolonflag,curx,cury);
}


/*
 * un-escaped character print routine
 */

void esc0 (unsigned char c)
{
    switch (c) {
    case 0:
	   /*
	 	* printing \000 on a terminal means "do nothing".
	 	* But since we use \000 as string terminator none
	 	* of the characters that follow were printed.
	 	*
	 	* perl -e 'printf("a%ca", 0);'
	 	*
	 	* said 'a' in a wterm, but should say 'aa'. This
	 	* bug screwed up most ncurses programs.
	 	*/
		break;
 
    case 7: /* bell */
		if (visualbell) {
			/* w_setmode(win, M_INVERS); */
			/* w_pbox(win, 0, 0, winw, winh); */
			/* w_test(win, 0, 0); */
			/* w_pbox(win, 0, 0, winw, winh); */
	    	;
		} else 
 	    	GrBell();
		break;

    case 8: /* backspace */
		hide_cursor();
		if ((curx -= 1) < 0)
		/* lineRedraw(); 
		if (--curx < 0) */
	    	curx = 0;
		pos_xaxis(curx);
		break;

    case 9: /* tab */
    	{
		int borg,i;

		borg = (((curx >> 3) + 1) << 3);
		if(borg >= col)
	    	borg = col-1;
		borg = borg-curx;
		for(i=0; i < borg; ++i)
			sadd(' ');
 		if ((curx = ((curx >> 3) + 1) << 3) >= col) 
 	    	curx = col - 1; 
		pos_xaxis(curx);
    	}
		break;

    case 10: /* line feed */
//GrError("\nLF\n");
		sflush();
		if (++cury >= scrollbottom) {
		//have to scroll before moving cursor, so reduce and add again
		cury--;
	    	vscrollup(1);
	    	cury++;
		//set cursor into lowest line (cury zero based so -1)
		cury = scrollbottom-1;
	    	//cury = row-1;
		}
		pos_yaxis(cury);
		break;

    case 13: /* carriage return */
		sflush();
		curx = 0;
		pos_xaxis(curx);
		break;

    case 27: /* escape */
		sflush();
		semicolonflag=0;
		escstate = 1;
		break;

    case 127: /* delete */
		break;

    default: /* any printable char */
		sadd(c);
		if (++curx >= col) {
	    	sflush();
	    	if (!wrap) 
				curx = col-1;
	    	else {
				curx = 0;
				if (++cury >= scrollbottom) 
		    		vscrollup(1);
	    	}
		}
		break;
    }
}


void printc(unsigned char c)
{
#if debug_screen
	GrError("%c,",c);
#endif
    switch(escstate) {
    case 0:
		esc0(c);
		break;

    case 1:
		sflush();
		esc1(c);
		break;

    case 2:
		sflush();
		esc2(c);
		break;

    case 3:
		sflush();
		esc3(c);
		break;

    case 4:
		sflush();
		esc4(c);
		break;

    case 5:
		sflush();
		esc5(c);
		break;

    case 10:
		sflush();
		esc100(c);
		break;

    default: 
		escstate = 0;
		break;
    }
}


void init(void)
{
    curx = savx = 0;
    cury = savy = 0;
    wrap = 1;
    curon = 1;
    curvis = 0;
    escstate = 0;
}


/*
 * general code...
 */
void
term(void)
{
	long 		in, l;
	GR_EVENT 	wevent;
	GR_EVENT_KEYSTROKE *kp;
	unsigned char 	buf[LARGEBUFFER];
	int			bufflen;


	if (prog_to_start[0]) {
		//enter program name from command line plus newline to call it now
		//GrError("prog_to_start:%s,len:%d\n",prog_to_start,strlen(prog_to_start));
		(void)write(pipeh,prog_to_start,strlen(prog_to_start));
	}

	while (42) {
		if (havefocus)
			draw_cursor();

		GrGetNextEvent(&wevent);

		switch(wevent.type) {
		case GR_EVENT_TYPE_CLOSE_REQ:
		  GrClose();
			exit(0);
			break;

		case GR_EVENT_TYPE_KEY_DOWN:
		  /* deal with special keys*/
			kp = (GR_EVENT_KEYSTROKE *)&wevent;
			int key = kp->ch;
			//printf("key-in:(%d) %X\n", key, key);
#if debug_kbd
	GrError("key-in:%X\n",kp->ch);
#endif
			if (kp->ch & MWKEY_NONASCII_MASK)
				if (termtype==0){
				bufflen = do_special_key_ansi(buf,kp->ch,kp->modifiers);
				} else {
				bufflen = do_special_key(buf,kp->ch,kp->modifiers);
				}
			else
			{
				*buf = kp->ch & 0xff;
				bufflen = 1;
			}
			if( bufflen > 0)
				(void)write(pipeh, buf, bufflen);
#if debug_kbd
	GrError("key-out:%X,%X,bufflen:%d\n",buf[0],buf[1],bufflen);
#endif
			break;

		case GR_EVENT_TYPE_FOCUS_IN:

			havefocus = GR_TRUE;
			break;

		case GR_EVENT_TYPE_FOCUS_OUT:

			havefocus = GR_FALSE;
			hide_cursor();
			break;

		case GR_EVENT_TYPE_UPDATE:
			/*
			 * if we get temporarily unmapped (moved),
			 * set cursor state off.
			 */
			if (wevent.update.utype == GR_UPDATE_UNMAPTEMP)
				curvis = 0;
			break;

		case GR_EVENT_TYPE_EXPOSURE:
			//screen is empty otherwise - so this workaround
		    //(void)write(pipeh,"clear\n",strlen("clear\n"));
			break;

		case GR_EVENT_TYPE_FDINPUT:
			hide_cursor();
			//while ((in = read(pipeh, buf, sizeof(buf))) > 0)
            if ((in = read(pipeh, buf, sizeof(buf))) > 0)
            {
				for (l=0; l<in; l++) {
					printc(buf[l]); 
				//	if (buf[l] == '\n')
				//		printc('\r');
				}
				sflush();
			}
	    	break;
		}
	}
}

int do_special_key(unsigned char *buffer, int key, int modifier)
//handle vt52 keys here
{
	int len;
	char *str, locbuff[256];

	switch (key) {
	case  MWKEY_LEFT:
		str="\033D";
		len = 2;
		break;
	case MWKEY_RIGHT:
		str="\033C";
		len=2;
		break;
	case MWKEY_UP:
		if(scrolledFlag) {
			str="";
			len = 0;
			scrolledFlag=0;
		} else {
			str="\033A";
			len=2;
		}
		break;
	case MWKEY_DOWN:
		str="\033B";
		len=2;
		break;
	case MWKEY_KP0:
		str="\033\077\160";
		len=3;
		break;
	case MWKEY_KP1:
		str="\033\077\161";
		len=3;
		break;
	case MWKEY_KP2:
		str="\033\077\162";
		len=3;
		break;		
	case MWKEY_KP3:
		str="\033\077\163";
		len=3;
		break;		
	case MWKEY_KP4:
		str="\033\077\164";
		len=3;
		break;		
	case MWKEY_KP5:
		str="\033\077\165";
		len=3;
		break;		
	case MWKEY_KP6:
		str="\033\077\166";
		len=3;
		break;		
	case MWKEY_KP7:
		str="\033\077\167";
		len=3;
		break;		
	case MWKEY_KP8:
		str="\033\077\170";
		len=3;
		break;		
	case MWKEY_KP9:
		str="\033\077\161";
		len=3;
		break;		
	case MWKEY_KP_PERIOD:
		str="\033\077\156";
		len=3;
		break;		
	case MWKEY_KP_ENTER:
		str="\033\077\115";
		len=3;
		break;
	case MWKEY_DELETE:
		str="\033C\177";
		len=3;
		break;
	case MWKEY_F1 ... MWKEY_F12:
		if ( modifier & MWKMOD_LMETA) {
			/* we set background color */
			locbuff[0]=033;
			locbuff[1]='c';
			locbuff[2]=(char)bgcolor[key - MWKEY_F1];
			str = locbuff;
			len=3;
		} else if ( modifier & MWKMOD_RMETA ) {
			/* we set foreground color */
			locbuff[0]=033;
			locbuff[1]='b';
			locbuff[2]=(char)fgcolor[key - MWKEY_F1];				
			str = locbuff;
			len=3;
		} else {
			switch (key) {
			case MWKEY_F1:
				str="\033Y";
				len=2;
				break;
			case MWKEY_F2:
				str="\033P";
				len=2;
				break;
			case MWKEY_F3:
				str="\033Q";
				len=2;
				break;
			case MWKEY_F4:
				str="\033R";
				len=2;
				break;
			case MWKEY_F5:
				str="\033S";
				len=2;
				break;
			case MWKEY_F6:
				str="\033T";
				len=2;
				break;
			case MWKEY_F7:
				str="\033U";
				len=2;
				break;
			case MWKEY_F8:
				str="\033V";
				len=2;
				break;
			case MWKEY_F9:
				str="\033W";
				len=2;
				break;
			case MWKEY_F10:
				str="\033X";
				len=2;
				break;
			}
		}
		/* fall thru*/

	default:
		str = "";
		len = 0;
	}
	if(len > 0)
		sprintf((char *)buffer,"%s",str);
	else
		buffer[0] = '\0';
	return len;
}

int do_special_key_ansi(unsigned char *buffer, int key, int modifier)
{
	int len;
	char *str, locbuff[256];

	switch (key) {
	case  MWKEY_LEFT:
		str="\033OD";
		len = 3;
		break;
	case MWKEY_RIGHT:
		str="\033OC";
		len=3;
		break;
	case MWKEY_UP:
		if(scrolledFlag) {
			str="";
			len = 0;
			scrolledFlag=0;
		} else {
			str="\033OA";
			len=3;
		}
		break;
	case MWKEY_DOWN:
		str="\033OB";
		len=3;
		break;
	case MWKEY_HOME:
		str="\033[1~";
		len=4;
		break;
	case MWKEY_INSERT:
		str="\033[2~";
		len=4;
		break;
	case MWKEY_KP0:
		str="\033Op";
		len=3;
		break;
	case MWKEY_END:
		str="\033[4~";
		len=4;
		break;
	case MWKEY_KP1:
		str="\033Oq";
		len=3;
		break;
	case MWKEY_KP2:
		str="\033Or";
		len=3;
		break;		
	case MWKEY_PAGEDOWN:	
		str="\033[6~";
		len=4;
		break;
	case MWKEY_KP3:
		str="\033Os";
		len=3;
		break;		
	case MWKEY_KP4:
		str="\033Ot";
		len=3;
		break;		
	case MWKEY_KP5:
		str="\033Ou";
		len=3;
		break;		
	case MWKEY_KP6:
		str="\033Ov";
		len=3;
		break;		
	case MWKEY_KP7:
		str="\033Ow";
		len=3;
		break;		
	case MWKEY_KP8:
		str="\033Ox";
		len=3;
		break;	
	case MWKEY_PAGEUP:	
		str="\033[5~";
		len=4;
		break;
	case MWKEY_KP9:
		str="\033Oy";
		len=3;
		break;		
/*
	case MWKEY_KP_PERIOD:
		str="\033On";
		len=3;
		break;		
*/
	case MWKEY_KP_ENTER:
		str="\033OM";
		len=3;
		break;
        case MWKEY_KP_PERIOD:
	case MWKEY_DELETE:
		str="\033[3~";
		len=4;
		break;
	case MWKEY_F1 ... MWKEY_F12:
		if ( modifier & MWKMOD_LMETA) {
			/* we set background color */
			locbuff[0]=033;
			locbuff[1]='c';
			locbuff[2]=(char)bgcolor[key - MWKEY_F1];
			str = locbuff;
			len=3;
		} else if ( modifier & MWKMOD_RMETA ) {
			/* we set foreground color */
			locbuff[0]=033;
			locbuff[1]='b';
			locbuff[2]=(char)fgcolor[key - MWKEY_F1];				
			str = locbuff;
			len=3;
		} else {
			switch (key) {
			case MWKEY_F1:
				str="\033OP";
				len=3;
				break;
			case MWKEY_F2:
				str="\033OQ";
				len=3;
				break;
			case MWKEY_F3:
				str="\033OR";
				len=3;
				break;
			case MWKEY_F4:
				str="\033OS";
				len=3;
				break;
			case MWKEY_F5:
				str="\033[15~";
				len=5;
				break;
			case MWKEY_F6:
				str="\033[17~"; //this is correct
				len=5;
				break;
			case MWKEY_F7:
				str="\033[18~";
				len=5;
				break;
			case MWKEY_F8:
				str="\033[19~";
				len=5;
				break;
			case MWKEY_F9:
				str="\033[20~";
				len=5;
				break;
			case MWKEY_F10:
				str="\033[21~";
				len=5;
				break;
			case MWKEY_F11:
				str="\033[22~";
				len=5;
				break;
			case MWKEY_F12:
				str="\033[23~";
				len=5;
				break;
			}
		}
		/* fall thru*/

	default:
		str = "";
		len = 0;
	}
	if(len > 0)
		sprintf((char *)buffer,"%s",str);
	else
		buffer[0] = '\0';
	return len;
}

void usage(char *s)
{
    if (s) GrError("error: %s\n", s);

    GrError("usage: term [-f <font family>] [-s <font size>]\n");
    GrError("       [-5] [-h] [program {args}]\n");
    GrError("       -f = select font, -s = font size -5 = vt52 mode\n");
    GrError("       -h = this help\n");
    exit(0);
}

#if UNIX
static void *mysignal(int signum, void *handler)
{
	struct sigaction sa, so;

	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(signum, &sa, &so);

	return so.sa_handler;
}

#if unused
void maximize(void)
{
    static short x0, y0, w, h, w_max,h_max;

    if (!isMaximized) {
		w_max=si.cols-wi.bordersize;
		h_max=si.rows-wi.bordersize;
		GrMoveWindow(w1,0,0);
		GrResizeWindow(w1,w_max, h_max);
		isMaximized=1;
    } else {
		GrResizeWindow(w1, w, h);
		GrMoveWindow(w1, x0, y0);
		isMaximized=0;
    }
}
#endif

static void sigpipe(int sig)
{
	/* this one musn't close the window */
	/*_write_utmp(pty, "", "", 0);  */
	kill(-pid, SIGHUP);
	_exit(sig);
}


static void sigchld(int sig)
{
	/*  _write_utmp(pty, "", "", 0);  */
	_exit(sig);
}


static void sigquit(int sig)
{
	signal(sig, SIG_IGN);
	kill(-pid, SIGHUP);
}
#endif /* UNIX*/

int main(int argc, char **argv)
{
    short xp, yp, fsize;
    short uid;
    char *family, *shell = NULL, *cptr, *geometry = NULL;
    char buf[stdcol];
    char thesh[128];
    GR_BITMAP	bitmap1fg[7];	/* mouse cursor */
    GR_BITMAP	bitmap1bg[7];
    GR_WM_PROPERTIES props;

#if UNIX
#ifdef SIGTTOU
    /* just in case we're started in the background */
    signal(SIGTTOU, SIG_IGN);
#endif

#endif

    if (GrOpen() < 0) {
		GrError("cannot open graphics\n");
		exit(1);
    }
    GrGetScreenInfo(&si);

    /*
     * scan arguments...
     */

    argv++;
    while (*argv && **argv=='-') 
	switch (*(*argv+1)) 
	{
	case '5': //vt52 mode
	    termtype = 1;
	    argv++;
	    break;

	case 'b':
	    cblink = 1;
	    argv++;
	    break;

	case 'd':
	    debug = 1;
	    argv++;
	    break;

	case 'f':
	    if (*++argv)
			family = *argv++;
	    else
			usage("-f option requires an argument");
	    break;

	case 's':
	    if (*++argv)
			fsize = atoi(*argv++);
	    else
			usage("-s option requires an argument");
	    break;

	case 'g':
	    if (*++argv)
			geometry = *argv++;
	    else
			usage("-g option requires an argument");
	    break;

	case 'h':
	    /* this will never return */
	    usage("");
		break;

	case 'v':
	    visualbell = 1;
	    argv++;
	    break;

	default:
	    usage("unknown option");
	}

#if UNIX
    /*
     * now *argv either points to a program to start or is zero
     */
#ifdef __FreeBSD__ 
	//now UNIX98 - shell is passed in FreeBSD only
    if (*argv)
		shell = *argv;
#else
    if (*argv) {
		while (*argv)
			sprintf(prog_to_start, "%s ", *argv++);
		strcat(prog_to_start,"\n");
	}
#endif

    if (!shell)
		shell = getenv("SHELL=");

    if (!shell)
		shell = "/initrd/shell";

    if (!*argv) {
		/*
		* the '-' makes the shell think it is a login shell,
		* we leave argv[0] alone if it isn`t a shell (ie.
		* the user specified the program to run as an argument
		* to wterm.
		*/
		cptr = strrchr(shell, '/');
		sprintf (thesh, "-%s", cptr ? cptr + 1 : shell);
		*--argv = thesh;
    }
#endif

    col = stdcol;
    row = stdrow;
    scrolltop=0;
    scrollbottom=row; //zero based
    xp = 0;
    yp = 0;
/*
    if (geometry) {
		if (col < 1) 
			col = stdcol;
		if (row < 1) 
			row = stdrow;
		if (col > 0x7f)
			colmask = 0xffff;
		if (row > 0x7f)
			rowmask = 0xffff;
    }
*/    
    regFont = GrCreateFontEx(GR_FONT_SYSTEM_FIXED, 0, 0, NULL);
    /*regFont = GrCreateFontEx(GR_FONT_OEM_FIXED, 0, 0, NULL);*/
    /*boldFont = GrCreateFontEx(GR_FONT_SYSTEM_FIXED, 0, 0, NULL);*/
    GrGetFontInfo(regFont, &fi);
    winw = col*fi.maxwidth;
    winh = (row+1)*fi.height;
    //w1 = GrNewWindow(GR_ROOT_WINDOW_ID, 10,10,winw, winh,0,BLACK,LTBLUE);
    w1 = GrNewWindow(GR_ROOT_WINDOW_ID, 10,10,winw, winh,0,stdbackground,stdforeground);
    props.flags = GR_WM_FLAGS_TITLE;
    props.title = TITLE;
    GrSetWMProperties(w1, &props);

    GrSelectEvents(w1, GR_EVENT_MASK_BUTTON_DOWN | GR_EVENT_MASK_EXPOSURE |
		   GR_EVENT_MASK_KEY_DOWN | 
		   GR_EVENT_MASK_FOCUS_IN | GR_EVENT_MASK_FOCUS_OUT |
		   GR_EVENT_MASK_UPDATE | GR_EVENT_MASK_CLOSE_REQ);
    GrMapWindow(w1);

    gc1 = GrNewGC();
    GrSetGCFont(gc1, regFont);

#define	_	((unsigned) 0)		/* off bits */
#define	X	((unsigned) 1)		/* on bits */
#define	MASK7(a,b,c,d,e,f,g) (((((((((((((a * 2) + b) * 2) + c) * 2) + d) * 2) + e) * 2) + f) * 2) + g) << 9)
	bitmap1fg[0] = MASK7(_,_,X,_,X,_,_);
	bitmap1fg[1] = MASK7(_,_,_,X,_,_,_);
	bitmap1fg[2] = MASK7(_,_,_,X,_,_,_);
	bitmap1fg[3] = MASK7(_,_,_,X,_,_,_);
	bitmap1fg[4] = MASK7(_,_,_,X,_,_,_);
	bitmap1fg[5] = MASK7(_,_,_,X,_,_,_);
	bitmap1fg[6] = MASK7(_,_,X,_,X,_,_);

	bitmap1bg[0] = MASK7(_,X,X,X,X,X,_);
	bitmap1bg[1] = MASK7(_,_,X,X,X,_,_);
	bitmap1bg[2] = MASK7(_,_,X,X,X,_,_);
	bitmap1bg[3] = MASK7(_,_,X,X,X,_,_);
	bitmap1bg[4] = MASK7(_,_,X,X,X,_,_);
	bitmap1bg[5] = MASK7(_,_,X,X,X,_,_);
	bitmap1bg[6] = MASK7(_,X,X,X,X,X,_);

    GrSetCursor(w1, 7, 7, 3, 3, stdforeground, stdbackground, bitmap1fg, bitmap1bg);
    GrSetGCForeground(gc1, stdforeground);
    GrSetGCBackground(gc1, stdbackground);
    GrGetWindowInfo(w1,&wi);
    GrGetGCInfo(gc1,&gi);

#if UNIX
	/*sprintf(buf, "wterm: %s", shell); */

if (termtype==1) { //set TERM and TERMCAP for vt52 only - default is ANSI or "linux"

    /*
     * what kind of terminal do we want to emulate?
     */
    putenv(termtype_string);		/* TERM=ngterm for Linux*/

    /*
     * this one should enable us to get rid of an /etc/termcap entry for
     * both curses and ncurses, hopefully...
     */

    if (termcap_string[0]) {		/* TERMCAP= string*/
		sprintf(termcap_string + strlen (termcap_string), "li#%d:co#%d:", row, col);
		putenv(termcap_string);
    }
} //termtype

    /* in case program absolutely needs terminfo entry, these 'should'
     * transmit the screen size of correctly (at least xterm sets these
     * and everything seems to work correctly...). Unlike putenv(),
     * setenv() allocates also the given string not just a pointer.
     */
    sprintf(buf, "%d", col);
    setenv("COLUMNS", buf, 1);
    sprintf(buf, "%d", row);
    setenv("LINES", buf, 1);


    /*
     * create a pty
     */
    pipeh = term_init();
	GrRegisterInput(pipeh);

	/*_write_utmp(pty, pw->pw_name, "", time(0)); */

	/*
	 * grantpt docs: "The behavior of grantpt() is unspecified if a signal handler
	 * is installed to catch SIGCHLD signals. "
	 */
    /* catch some signals */
    mysignal(SIGTERM, sigquit);
    mysignal(SIGHUP, sigquit);
    mysignal(SIGINT, SIG_IGN);
    mysignal(SIGQUIT, sigquit);
    mysignal(SIGPIPE, sigpipe);
    mysignal(SIGCHLD, sigchld);

#endif /* UNIX*/

    init();
    term();
    return 0;
}

#if UNIX
/* 
 * pty create/open routines
 */


void sigchild(int signo)
{
	GrClose();
	exit(0);
}

extern char **environ;

int term_init(void)
{
	int tfd;
	pid_t pid;
	char ptyname[50];
	
	tfd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (tfd < 0) goto err;
    /*
	signal(SIGCHLD, SIG_DFL);	// required before grantpt()
	if (grantpt(tfd) || unlockpt(tfd)) goto err; 
	signal(SIGCHLD, sigchild);
	signal(SIGINT, sigchild);
    */

    ptsname_r(tfd, ptyname, 50);

	char* argv[2];
    argv[0] = "/initrd/shell";
    argv[1] = NULL;

    execute_on_tty(argv[0], argv, environ, ptyname);

	return tfd;
err:
	GrError("Can't create pty\n");
	return -1;	
}


#endif /* UNIX*/
