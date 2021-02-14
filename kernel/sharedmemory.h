#ifndef SHAREDMEMORY_H
#define SHAREDMEMORY_H

#include "common.h"
#include "fs.h"

void sharedmemory_initialize();
FileSystemNode* sharedmemory_create(const char* name);
BOOL sharedmemory_destroy_by_name(const char* name);
FileSystemNode* sharedmemory_get_node(const char* name);
BOOL sharedmemory_unmap_if_exists(Process* process, uint32_t address);
void sharedmemory_unmap_for_process_all(Process* process);

#endif // SHAREDMEMORY_H
