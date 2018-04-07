#ifndef SOSOUSDK_H
#define SOSOUSDK_H

#include "commonuser.h"

unsigned int createWindow(unsigned short width, unsigned short height);
void destroyWindow(unsigned int windowHandle);
void setWindowPosition(unsigned int windowHandle, unsigned short x, unsigned short y);
void copyToWindowBuffer(unsigned int windowHandle, const char* buffer);
void drawCharAt(unsigned char* windowBuffer, unsigned short int c, int cx, int cy, int windowWidth, int windowHeight, unsigned int fg, unsigned int bg);

unsigned int getUptimeMilliseconds();
void sleepMilliseconds(unsigned int ms);
int executeOnTTY(const char *path, char *const argv[], char *const envp[], const char *ttyPath);
void sendCharacterToTTY(int fd, char c);
int getMessageQueueCount();
int getNextMessage(SosoMessage* message);
int getTTYBufferSize(int fd);
int getTTYBuffer(int fd, TtyUserBuffer* ttyBuffer);
int setTTYBuffer(int fd, TtyUserBuffer* ttyBuffer);

#endif //SOSOUSDK_H
