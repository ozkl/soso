#ifndef MESSAGE_H
#define MESSAGE_H

#include "common.h"

typedef struct Thread Thread;

typedef struct SosoMessage
{
    uint32 messageType;
    int32 parameter1;
    int32 parameter2;
    int32 parameter3;
} SosoMessage;

void sendMesageKeyEvent(Thread* thread, uint8 scancode);

uint32 getMessageQueueCount(Thread* thread);

//returns remaining message count
int32 getNextMessage(Thread* thread, SosoMessage* message);

#endif // MESSAGE_H
