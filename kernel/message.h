#ifndef MESSAGE_H
#define MESSAGE_H

#include "common.h"
#include "commonuser.h"

typedef struct Thread Thread;

void message_send(Thread* thread, SosoMessage* message);

uint32_t message_get_queue_count(Thread* thread);

//returns remaining message count
int32_t message_get_next(Thread* thread, SosoMessage* message);

#endif // MESSAGE_H
