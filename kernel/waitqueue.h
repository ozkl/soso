/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#ifndef WAITQUEUE_H
#define WAITQUEUE_H

#include "common.h"
#include "list.h"

typedef struct Thread Thread;

typedef struct WaitQueue
{
    List* threads;
} WaitQueue;

void waitqueue_initialize(WaitQueue* queue);
void waitqueue_destroy(WaitQueue* queue);
void waitqueue_wait(WaitQueue* queue, void* private_data);
void waitqueue_wake_all(WaitQueue* queue, void* private_data);
void waitqueue_remove_thread(Thread* thread);
BOOL waitqueue_is_empty(WaitQueue* queue);

#endif //WAITQUEUE_H
