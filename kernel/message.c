#include "message.h"
#include "process.h"
#include "fifobuffer.h"


void message_send(Thread* thread, SosoMessage* message)
{
    Spinlock_Lock(&(thread->message_queue_lock));

    fifobuffer_enqueue(thread->message_queue, (uint8*)message, sizeof(SosoMessage));

    Spinlock_Unlock(&(thread->message_queue_lock));
}

uint32 message_get_queue_count(Thread* thread)
{
    int result = 0;

    Spinlock_Lock(&(thread->message_queue_lock));

    result = fifobuffer_get_size(thread->message_queue) / sizeof(SosoMessage);

    Spinlock_Unlock(&(thread->message_queue_lock));

    return result;
}

//returns remaining message count
int32 message_get_next(Thread* thread, SosoMessage* message)
{
    uint32 result = -1;

    Spinlock_Lock(&(thread->message_queue_lock));

    result = fifobuffer_get_size(thread->message_queue) / sizeof(SosoMessage);

    if (result > 0)
    {
        fifobuffer_dequeue(thread->message_queue, (uint8*)message, sizeof(SosoMessage));

        --result;
    }
    else
    {
        result = -1;
    }

    Spinlock_Unlock(&(thread->message_queue_lock));

    return result;
}
