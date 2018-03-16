#ifndef TTYDRIVER_H
#define TTYDRIVER_H

typedef struct FifoBuffer FifoBuffer;

void initializeTTYs();

FifoBuffer* getTTYDriverKeyBuffer();

//TODO: posix_openpt

#endif // TTYDRIVER_H
