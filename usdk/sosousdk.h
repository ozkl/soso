#ifndef SOSOUSDK_H
#define SOSOUSDK_H

typedef struct SosoMessage
{
    unsigned int messageType;
    int parameter1;
    int parameter2;
    int parameter3;
} SosoMessage;

unsigned int createWindow(unsigned short width, unsigned short height);
void destroyWindow(unsigned int windowHandle);
void setWindowPosition(unsigned int windowHandle, unsigned short x, unsigned short y);
void copyToWindowBuffer(unsigned int windowHandle, const char* buffer);
void drawCharAt(unsigned char* windowBuffer, unsigned short int c, int cx, int cy, int windowWidth, int windowHeight, unsigned int fg, unsigned int bg);

unsigned int getUptimeMilliseconds();
void sleepMilliseconds(unsigned int ms);
int executeOnTTY(const char *path, char *const argv[], char *const envp[], const char *ttyPath);

int getMessageQueueCount();
int getNextMessage(SosoMessage* message);

#endif //SOSOUSDK_H
