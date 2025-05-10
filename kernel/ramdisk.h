#ifndef RAMDISK_H
#define RAMDISK_H

#include "common.h"

BOOL ramdisk_create(const char* devName, uint32_t size, uint8_t* preallocated_buffer);

#endif // RAMDISK_H
