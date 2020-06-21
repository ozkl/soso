#ifndef SOSOUSDK_H
#define SOSOUSDK_H

#include "commonuser.h"


unsigned int getUptimeMilliseconds();
void sleepMilliseconds(unsigned int ms);
int executeOnTTY(const char *path, char *const argv[], char *const envp[], const char *ttyPath);
void sendCharacterToTTY(int fd, char c);
void sendMessage(SosoMessage* message);
int getMessageQueueCount();
int getNextMessage(SosoMessage* message);
int getTTYBufferSize(int fd);
int getTTYBuffer(int fd, TtyUserBuffer* ttyBuffer);
int setTTYBuffer(int fd, TtyUserBuffer* ttyBuffer);
int getdents(int fd, char *buf, int nbytes);
int execute(const char *path, char *const argv[], char *const envp[]);
int executep(const char *filename, char *const argv[], char *const envp[]);
int getWorkingDirectory(char *buf, int size);
int setWorkingDirectory(const char *path);


#endif //SOSOUSDK_H
