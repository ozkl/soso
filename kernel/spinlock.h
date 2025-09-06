/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#ifndef SPINLOCK_H
#define SPINLOCK_H

#include "common.h"

typedef int32_t Spinlock;

void spinlock_init(Spinlock* spinlock);
void spinlock_lock(Spinlock* spinlock);
BOOL spinlock_try_lock(Spinlock* spinlock);
void spinlock_unlock(Spinlock* spinlock);

#endif // SPINLOCK_H
