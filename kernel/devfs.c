#include "devfs.h"
#include "common.h"
#include "fs.h"
#include "alloc.h"
#include "device.h"
#include "screen.h"

static FileSystemNode* gDevRoot = NULL;

static FileSystemNode* gDeviceList = NULL;
static int gNextDeviceIndex = 0;

#define MAX_DEVICE_COUNT 200

static BOOL devfs_open(File *node, uint32 flags);
static FileSystemDirent *devfs_readdir(FileSystemNode *node, uint32 index);
static FileSystemNode *devfs_finddir(FileSystemNode *node, char *name);

static FileSystemDirent gDirent;

void initializeDevFS()
{
    gDevRoot = kmalloc(sizeof(FileSystemNode));
    memset((uint8*)gDevRoot, 0, sizeof(FileSystemNode));

    gDevRoot->nodeType = FT_Directory;

    FileSystemNode* rootFs = getFileSystemRootNode();

    FileSystemNode* devNode = finddir_fs(rootFs, "dev");

    if (devNode)
    {
        devNode->nodeType |= FT_MountPoint;
        devNode->mountPoint = gDevRoot;
        gDevRoot->parent = devNode->parent;
        strcpy(gDevRoot->name, devNode->name);
    }
    else
    {
        PANIC("/dev does not exist!");
    }

    gDevRoot->open = devfs_open;
    gDevRoot->finddir = devfs_finddir;
    gDevRoot->readdir = devfs_readdir;

    gDeviceList = kmalloc(sizeof(FileSystemNode) * MAX_DEVICE_COUNT);
    memset((uint8*)gDeviceList, 0, sizeof(FileSystemNode) * MAX_DEVICE_COUNT);
}

static BOOL devfs_open(File *node, uint32 flags)
{
    return TRUE;
}

static FileSystemDirent *devfs_readdir(FileSystemNode *node, uint32 index)
{
    if (/*index >= 0 &&*/ index < gNextDeviceIndex)
    {
        FileSystemNode* deviceNode = &gDeviceList[index];
        strcpy(gDirent.name, deviceNode->name);
        gDirent.fileType = deviceNode->nodeType;
        gDirent.inode = index;
        return &gDirent;
    }
    return NULL;
}

static FileSystemNode *devfs_finddir(FileSystemNode *node, char *name)
{
    //Screen_PrintF("devfs_finddir\n");

    for (int i = 0; i < gNextDeviceIndex; ++i)
    {
        FileSystemNode* deviceNode = &gDeviceList[i];

        if (strcmp(name, deviceNode->name) == 0)
        {
            return deviceNode;
        }
    }

    return NULL;
}

BOOL registerDevice(Device* device)
{
    if (gNextDeviceIndex >= MAX_DEVICE_COUNT)
    {
        WARNING("Registered device list is full!\n");

        return FALSE;
    }

    for (int i = 0; i < gNextDeviceIndex; ++i)
    {
        FileSystemNode* deviceNode = &gDeviceList[i];

        if (strcmp(device->name, deviceNode->name) == 0)
        {
            //There is already a device with the same name
            return FALSE;
        }
    }

    FileSystemNode* deviceNode = &gDeviceList[gNextDeviceIndex++];
    memset((uint8*)deviceNode, 0, sizeof(FileSystemNode));
    strcpy(deviceNode->name, device->name);
    deviceNode->nodeType = device->deviceType;
    deviceNode->open = device->open;
    deviceNode->close = device->close;
    deviceNode->readBlock = device->readBlock;
    deviceNode->writeBlock = device->writeBlock;
    deviceNode->read = device->read;
    deviceNode->write = device->write;
    deviceNode->ioctl = device->ioctl;
    deviceNode->privateNodeData = device->privateData;
    deviceNode->parent = gDevRoot;

    return TRUE;
}
