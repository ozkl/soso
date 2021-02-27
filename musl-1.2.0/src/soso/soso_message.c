#include <stdint.h>
#include "syscall.h"
#include "soso.h"

int32_t manage_message(int32_t command, SosoMessage* message)
{
    return __syscall(SYS_manage_message, command, (int)message);
}

int get_message_count()
{
    return manage_message(0, 0);
}

void send_message(SosoMessage* message)
{
    manage_message(1, message);
}

int get_next_message(SosoMessage* message)
{
    return manage_message(2, message);
}