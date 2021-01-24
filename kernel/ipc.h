#ifndef IPC_H
#define IPC_H

#include "stdint.h"

#define IPC_CREAT  01000
#define IPC_EXCL   02000
#define IPC_NOWAIT 04000

#define IPC_RMID 0
#define IPC_SET  1
#define IPC_INFO 3

#define IPC_PRIVATE ((int32_t) 0)

#endif //IPC_H