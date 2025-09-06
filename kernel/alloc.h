/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#ifndef ALLOC_H
#define ALLOC_H

#include "common.h"
#include "process.h"

void initialize_kernel_heap();
void *ksbrk_page(int n);
void *kmalloc(uint32_t size);
void kfree(void *v_addr);

void initialize_program_break(Process* process, uint32_t begin, uint32_t size);
void *sbrk(Process* process, int n_bytes);

uint32_t get_kernel_heap_used();

struct MallocHeader
{
    unsigned long size:31;
    unsigned long used:1;
} __attribute__ ((packed));

typedef struct MallocHeader MallocHeader;

#endif // ALLOC_H
