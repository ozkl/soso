#ifndef PIPE_H
#define PIPE_H

#include "common.h"

void pipe_initialize();
BOOL pipe_create(const char* name, uint32_t bufferSize);
BOOL pipe_destroy(const char* name);
BOOL pipe_exists(const char* name);

#endif // PIPE_H
