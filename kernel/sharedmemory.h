#ifndef SHAREDMEMORY_H
#define SHAREDMEMORY_H

#include "common.h"
#include "fs.h"

void sharedmemory_initialize();
FileSystemNode* sharedmemory_create(const char* name);
BOOL sharedmemory_destroy_by_name(const char* name);
FileSystemNode* sharedmemory_get_node(const char* name);

#endif // SHAREDMEMORY_H
