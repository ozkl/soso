/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#include "waitqueue.h"
#include "process.h"

void waitqueue_initialize(WaitQueue* queue)
{
    queue->threads = list_create();
}

void waitqueue_destroy(WaitQueue* queue)
{
    list_destroy(queue->threads);
    queue->threads = NULL;
}

void waitqueue_remove_thread(Thread* thread)
{
    begin_critical_section();

    if (thread->wait_queue == NULL)
    {
        end_critical_section();
        return;
    }

    list_remove_first_occurrence(thread->wait_queue->threads, thread);
    thread->wait_queue = NULL;

    end_critical_section();
}

static void waitqueue_wait_internal(WaitQueue* queue, void* private_data, uint64_t target_time)
{
    disable_interrupts();

    Thread* thread = thread_get_current();
    waitqueue_remove_thread(thread);
    list_append(queue->threads, thread);
    thread->wait_queue = queue;
    thread->wait_target_time = target_time;
    thread_change_state(thread, TS_WAITIO, private_data);

    enable_interrupts();
    halt();

    disable_interrupts();
    waitqueue_remove_thread(thread);
}

void waitqueue_wait(WaitQueue* queue, void* private_data)
{
    waitqueue_wait_internal(queue, private_data, 0);
}

void waitqueue_wait_until(WaitQueue* queue, void* private_data, uint64_t target_time)
{
    waitqueue_wait_internal(queue, private_data, target_time);
}

void waitqueue_wake_all(WaitQueue* queue, void* private_data)
{
    ListNode* node = list_get_first_node(queue->threads);
    while (node != NULL)
    {
        ListNode* next = node->next;
        Thread* thread = (Thread*)node->data;

        if (!thread_is_valid(thread))
        {
            list_remove_node(queue->threads, node);
        }
        else if (thread->wait_queue != queue ||
                 thread->state != TS_WAITIO)
        {
            list_remove_node(queue->threads, node);
            if (thread->wait_queue == queue)
            {
                thread->wait_queue = NULL;
            }
        }
        else if (thread->state_privateData == private_data)
        {
            list_remove_node(queue->threads, node);
            thread->wait_queue = NULL;
            thread_resume(thread);
        }

        node = next;
    }
}

BOOL waitqueue_is_empty(WaitQueue* queue)
{
    return list_is_empty(queue->threads);
}
