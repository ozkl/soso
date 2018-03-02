#ifndef PIPE_H
#define PIPE_H

#include "common.h"

typedef struct List List;


void initializePipes();
List* getPipeList();
BOOL createPipe(const char* name);
BOOL destroyPipe(const char* name);

#endif // PIPE_H
