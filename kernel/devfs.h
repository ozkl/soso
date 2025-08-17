#ifndef DEVFS_H
#define DEVFS_H

#include "device.h"
#include "fs.h"
#include "common.h"

void devfs_initialize();
FileSystemNode* devfs_register_device(Device* device, BOOL add_to_fs);
void devfs_unregister_device(FileSystemNode* device_node);

#endif // DEVFS_H
