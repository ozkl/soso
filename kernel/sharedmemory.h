#ifndef SHAREDMEMORY_H
#define SHAREDMEMORY_H

#include "common.h"
#include "fs.h"

void sharedmemory_initialize();
FileSystemNode* sharedmemory_create(const char* name);
void sharedmemory_destroy(const char* name);
FileSystemNode* sharedmemory_get_node(const char* name);

#endif // SHAREDMEMORY_H
