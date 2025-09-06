/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#include "spinlock.h"

static inline int32_t exchange_atomic(volatile int32_t* old_value_address, int32_t new_value)
{
    //no need to use lock instruction on xchg

    asm volatile ("xchgl %0, %1"
                   : "=r"(new_value)
                   : "m"(*old_value_address), "0"(new_value)
                   : "memory");
    return new_value;
}

void spinlock_init(Spinlock* spinlock)
{
    *spinlock = 0;
}

void spinlock_lock(Spinlock* spinlock)
{
    while (exchange_atomic((int32_t*)spinlock, 1))
    {
        halt();
    }
}

BOOL spinlock_try_lock(Spinlock* spinlock)
{
    if (exchange_atomic((int32_t*)spinlock, 1))
    {
        return FALSE;
    }

    return TRUE;
}

void spinlock_unlock(Spinlock* spinlock)
{
    *spinlock = 0;
}
