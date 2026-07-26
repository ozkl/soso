/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#ifndef SHAREDMEMORY_H
#define SHAREDMEMORY_H

#include "common.h"
#include "fs.h"

#define SHM_SEAL_SEAL   0x0001
#define SHM_SEAL_SHRINK 0x0002
#define SHM_SEAL_GROW   0x0004

void sharedmemory_initialize();
FileSystemNode* sharedmemory_create(const char* name);
FileSystemNode* sharedmemory_create_anonymous(const char* name, BOOL allow_sealing);
BOOL sharedmemory_destroy_by_name(const char* name);
FileSystemNode* sharedmemory_get_node(const char* name);
int32_t sharedmemory_add_seals(File* file, uint32_t seals);
int32_t sharedmemory_get_seals(File* file);
int32_t sharedmemory_unmap_if_exists(Process* process, uint32_t address, uint32_t page_count);
void sharedmemory_unmap_for_process_all(Process* process);

#endif // SHAREDMEMORY_H
