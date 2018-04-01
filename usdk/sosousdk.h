#ifndef SOSOUSDK_H
#define SOSOUSDK_H

unsigned int createWindow(unsigned short width, unsigned short height);
void destroyWindow(unsigned int windowHandle);
void setWindowPosition(unsigned int windowHandle, unsigned short x, unsigned short y);
void copyToWindowBuffer(unsigned int windowHandle, const char* buffer);
void drawCharAt(unsigned char* windowBuffer, unsigned short int c, int cx, int cy, int windowWidth, int windowHeight, unsigned int fg, unsigned int bg);

#endif //SOSOUSDK_H
