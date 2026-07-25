/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#include "common.h"
#include "fpu.h"
#include "process.h"

#define CPUID_FXSR (1 << 24)
#define CPUID_SSE  (1 << 25)
#define CPUID_SSE2 (1 << 26)

#define CR0_MP (1 << 1)
#define CR0_EM (1 << 2)
#define CR0_TS (1 << 3)
#define CR0_NE (1 << 5)

#define CR4_OSFXSR     (1 << 9)
#define CR4_OSXMMEXCPT (1 << 10)

static uint8_t g_initial_fpu_state[512 + 15];

static void* align_fpu_state(uint8_t* state)
{
    return (void*)(((uint32_t)state + 15) & ~15);
}

void fpu_initialize()
{
    uint32_t eax = 1;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;

    asm volatile("cpuid"
        : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));

    if ((edx & (CPUID_FXSR | CPUID_SSE | CPUID_SSE2)) !=
        (CPUID_FXSR | CPUID_SSE | CPUID_SSE2))
    {
        PANIC("FXSR, SSE and SSE2 are required");
    }

    asm volatile(
        "mov %%cr0, %%eax;"
        "and %0, %%eax;"
        "or %1, %%eax;"
        "mov %%eax, %%cr0;"
        "mov %%cr4, %%eax;"
        "or %2, %%eax;"
        "mov %%eax, %%cr4"
        :
        : "i"(~(CR0_EM | CR0_TS)),
          "i"(CR0_MP | CR0_NE),
          "i"(CR4_OSFXSR | CR4_OSXMMEXCPT)
        : "eax", "memory");

    asm volatile("fninit");

    uint32_t mxcsr = 0x1F80;
    asm volatile("ldmxcsr %0" :: "m"(mxcsr));
    asm volatile("fxsave %0" : "=m"(*(uint8_t (*)[512])align_fpu_state(g_initial_fpu_state)));
}

void fpu_initialize_thread(Thread* thread)
{
    memcpy(align_fpu_state(thread->fpu_state),
           align_fpu_state(g_initial_fpu_state), 512);
}

void fpu_clone_thread(Thread* destination, Thread* source)
{
    memcpy(align_fpu_state(destination->fpu_state),
           align_fpu_state(source->fpu_state), 512);
}

void fpu_save(Thread* thread)
{
    asm volatile("fxsave %0"
        : "=m"(*(uint8_t (*)[512])align_fpu_state(thread->fpu_state)));
}

void fpu_restore(Thread* thread)
{
    asm volatile("fxrstor %0"
        :
        : "m"(*(uint8_t (*)[512])align_fpu_state(thread->fpu_state)));
}
