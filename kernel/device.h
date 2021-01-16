#ifndef DEVICE_H
#define DEVICE_H

#include "common.h"
#include "fs.h"

typedef struct Device
{
    char name[16];
    FileType device_type;
    ReadWriteBlockFunction read_block;
    ReadWriteBlockFunction write_block;
    ReadWriteFunction read;
    ReadWriteFunction write;
    ReadWriteTestFunction read_test_ready;
    ReadWriteTestFunction write_test_ready;
    OpenFunction open;
    CloseFunction close;
    IoctlFunction ioctl;
    FtruncateFunction ftruncate;
    MmapFunction mmap;
    MunmapFunction munmap;
    void * private_data;
} Device;

#endif // DEVICE_H
