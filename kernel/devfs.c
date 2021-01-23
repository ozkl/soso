#include "devfs.h"
#include "common.h"
#include "fs.h"
#include "alloc.h"
#include "device.h"
#include "list.h"
#include "spinlock.h"

static FileSystemNode* g_dev_root = NULL;

static List* g_device_list = NULL;
static Spinlock g_device_list_lock;

static BOOL devfs_open(File *node, uint32_t flags);
static FileSystemDirent *devfs_readdir(FileSystemNode *node, uint32_t index);
static FileSystemNode *devfs_finddir(FileSystemNode *node, char *name);

static FileSystemDirent g_dirent;

void devfs_initialize()
{
    g_dev_root = kmalloc(sizeof(FileSystemNode));
    memset((uint8_t*)g_dev_root, 0, sizeof(FileSystemNode));

    g_dev_root->node_type = FT_DIRECTORY;

    FileSystemNode* root_node = fs_get_root_node();

    FileSystemNode* dev_node = fs_finddir(root_node, "dev");

    if (dev_node)
    {
        dev_node->node_type |= FT_MOUNT_POINT;
        dev_node->mount_point = g_dev_root;
        g_dev_root->parent = dev_node->parent;
        strcpy(g_dev_root->name, dev_node->name);
    }
    else
    {
        PANIC("/dev does not exist!");
    }

    g_dev_root->open = devfs_open;
    g_dev_root->finddir = devfs_finddir;
    g_dev_root->readdir = devfs_readdir;

    g_device_list = list_create();
    spinlock_init(&g_device_list_lock);
}

static BOOL devfs_open(File *node, uint32_t flags)
{
    return TRUE;
}

static FileSystemDirent *devfs_readdir(FileSystemNode *node, uint32_t index)
{
    FileSystemDirent * result = NULL;

    uint32_t counter = 0;

    spinlock_lock(&g_device_list_lock);

    list_foreach(n, g_device_list)
    {
        if (index == counter)
        {
            FileSystemNode* device_node = (FileSystemNode*)n->data;
            strcpy(g_dirent.name, device_node->name);
            g_dirent.file_type = device_node->node_type;
            g_dirent.inode = index;
            result = &g_dirent;
            break;
        }

        ++counter;
    }
    spinlock_unlock(&g_device_list_lock);

    return result;
}

static FileSystemNode *devfs_finddir(FileSystemNode *node, char *name)
{
    FileSystemNode* result = NULL;


    spinlock_lock(&g_device_list_lock);

    list_foreach(n, g_device_list)
    {
        FileSystemNode* deviceNode = (FileSystemNode*)n->data;

        if (strcmp(name, deviceNode->name) == 0)
        {
            result = deviceNode;
            break;
        }
    }

    spinlock_unlock(&g_device_list_lock);

    return result;
}

FileSystemNode* devfs_register_device(Device* device)
{
    spinlock_lock(&g_device_list_lock);

    list_foreach(n, g_device_list)
    {
        FileSystemNode* device_node = (FileSystemNode*)n->data;

        if (strcmp(device->name, device_node->name) == 0)
        {
            //There is already a device with the same name
            spinlock_unlock(&g_device_list_lock);
            return NULL;
        }
    }

    FileSystemNode* device_node = (FileSystemNode*)kmalloc(sizeof(FileSystemNode));
    memset((uint8_t*)device_node, 0, sizeof(FileSystemNode));
    strcpy(device_node->name, device->name);
    device_node->node_type = device->device_type;
    device_node->open = device->open;
    device_node->close = device->close;
    device_node->read_block = device->read_block;
    device_node->write_block = device->write_block;
    device_node->read = device->read;
    device_node->write = device->write;
    device_node->read_test_ready = device->read_test_ready;
    device_node->write_test_ready = device->write_test_ready;
    device_node->ioctl = device->ioctl;
    device_node->ftruncate = device->ftruncate;
    device_node->mmap = device->mmap;
    device_node->munmap = device->munmap;
    device_node->private_node_data = device->private_data;
    device_node->parent = g_dev_root;

    list_append(g_device_list, device_node);

    spinlock_unlock(&g_device_list_lock);

    return device_node;
}
