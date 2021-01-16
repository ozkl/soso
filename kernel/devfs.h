#ifndef DEVFS_H
#define DEVFS_H

#include "device.h"
#include "fs.h"
#include "common.h"

void devfs_initialize();
FileSystemNode* devfs_register_device(Device* device);

#endif // DEVFS_H
