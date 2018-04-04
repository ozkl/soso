#include "message.h"
#include "process.h"
#include "fifobuffer.h"

#include "sosomessage.h"

void sendMesageKeyEvent(Thread* thread, uint8 scancode)
{
    SosoMessage message;
    message.messageType = SM_KEYEVENT;
    message.parameter1 = scancode;
    message.parameter2 = 0;
    message.parameter3 = 0;

    Spinlock_Lock(&(thread->messageQueueLock));

    FifoBuffer_enqueue(thread->messageQueue, (uint8*)&message, sizeof(SosoMessage));

    Spinlock_Unlock(&(thread->messageQueueLock));
}

uint32 getMessageQueueCount(Thread* thread)
{
    int result = 0;

    Spinlock_Lock(&(thread->messageQueueLock));

    result = FifoBuffer_getSize(thread->messageQueue) / sizeof(SosoMessage);

    Spinlock_Unlock(&(thread->messageQueueLock));

    return result;
}

//returns remaining message count
int32 getNextMessage(Thread* thread, SosoMessage* message)
{
    uint32 result = -1;

    Spinlock_Lock(&(thread->messageQueueLock));

    result = FifoBuffer_getSize(thread->messageQueue) / sizeof(SosoMessage);

    if (result > 0)
    {
        FifoBuffer_dequeue(thread->messageQueue, (uint8*)message, sizeof(SosoMessage));

        --result;
    }

    Spinlock_Unlock(&(thread->messageQueueLock));

    return result;
}
