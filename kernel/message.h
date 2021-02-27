#ifndef MESSAGE_H
#define MESSAGE_H

#include "common.h"

typedef struct SosoMessage
{
    uint32_t message_type;
    uint32_t parameter1;
    uint32_t parameter2;
    uint32_t parameter3;
} SosoMessage;

typedef struct Thread Thread;

void message_send(Thread* thread, SosoMessage* message);

uint32_t message_get_queue_count(Thread* thread);

//returns remaining message count
int32_t message_get_next(Thread* thread, SosoMessage* message);

#endif // MESSAGE_H
