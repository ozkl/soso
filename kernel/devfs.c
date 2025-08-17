#include "devfs.h"
#include "common.h"
#include "fs.h"
#include "alloc.h"
#include "device.h"
#include "list.h"
#include "spinlock.h"

static FileSystemNode* g_dev_root = NULL;

static BOOL devfs_open(File *node, uint32_t flags);
static FileSystemDirent *devfs_readdir(FileSystemNode *node, uint32_t index);
static FileSystemNode *devfs_finddir(FileSystemNode *node, char *name);
static BOOL devfs_mkdir(FileSystemNode *node, const char *name, uint32_t flags);

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
    g_dev_root->mkdir = devfs_mkdir;
}

static BOOL devfs_open(File *node, uint32_t flags)
{
    return TRUE;
}

static FileSystemDirent *devfs_readdir(FileSystemNode *node, uint32_t index)
{
    FileSystemNode *n = node->first_child;
    uint32_t i = 0;
    while (NULL != n)
    {
        if (index == i)
        {
            g_dirent.file_type = n->node_type;
            g_dirent.inode = n->inode;
            strcpy(g_dirent.name, n->name);

            return &g_dirent;
        }
        n = n->next_sibling;
        ++i;
    }

    return NULL;
}

static FileSystemNode *devfs_finddir(FileSystemNode *node, char *name)
{
    FileSystemNode *n = node->first_child;
    while (NULL != n)
    {
        if (strcmp(name, n->name) == 0)
        {
            return n;
        }
        n = n->next_sibling;
    }

    return NULL;
}

static BOOL devfs_mkdir(FileSystemNode *node, const char *name, uint32_t flags)
{
    FileSystemNode *n = node->first_child;
    while (NULL != n)
    {
        if (strcmp(name, n->name) == 0)
        {
            return FALSE;
        }
        n = n->next_sibling;
    }

    FileSystemNode* new_node = (FileSystemNode*)kmalloc(sizeof(FileSystemNode));
    memset((uint8_t*)new_node, 0, sizeof(FileSystemNode));
    strcpy(new_node->name, name);
    new_node->node_type = FT_DIRECTORY;
    new_node->open = devfs_open;
    new_node->readdir = devfs_readdir;
    new_node->finddir = devfs_finddir;
    new_node->mkdir = devfs_mkdir;
    new_node->parent = node;

    if (node->first_child == NULL)
    {
        node->first_child = new_node;
    }
    else
    {
        FileSystemNode *n = node->first_child;
        while (NULL != n->next_sibling)
        {
            n = n->next_sibling;
        }
        n->next_sibling = new_node;
    }

    return TRUE;
}

static FileSystemNode *devfs_create_device(Device *device, const char *name)
{
    FileSystemNode* device_node = (FileSystemNode*)kmalloc(sizeof(FileSystemNode));
    memset((uint8_t*)device_node, 0, sizeof(FileSystemNode));
    strcpy(device_node->name, name);
    device_node->node_type = device->device_type;
    device_node->open = device->open;
    device_node->create = device->create;
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

    return device_node;
}

static void devfs_add_device_under_node(FileSystemNode* device_node, FileSystemNode *node)
{
    device_node->parent = node;

    if (node->first_child == NULL)
    {
        node->first_child = device_node;
    }
    else
    {
        FileSystemNode *n = node->first_child;
        while (NULL != n->next_sibling)
        {
            n = n->next_sibling;
        }
        n->next_sibling = device_node;
    }
}

FileSystemNode* devfs_register_device(Device* device, BOOL add_to_fs)
{
    FileSystemNode *node = g_dev_root;
    char *path = device->name;
    char path_last[64];
    while (path)
    {
        BOOL is_dir = FALSE;
        memset((uint8_t*)path_last, 0, sizeof(path_last));

        char *next_slash = strstr(path, "/");
        if (next_slash)
        {
            is_dir = TRUE;

            strncpy_null(path_last, path, next_slash - path + 1);

            path = next_slash + 1;
        }
        else
        {
            strcpy(path_last, path);

            path = NULL;
        }
        

        FileSystemNode *n = fs_finddir(node, path_last);

        if (is_dir)
        {
            if (NULL == n)
            {
                if (fs_mkdir(node, path_last, 0))
                {
                    n = fs_finddir(node, path_last);
                }
            }
        }
        else
        {
            if (NULL == n)
            {
                //create device
                FileSystemNode* device_node = devfs_create_device(device, path_last);
                if (add_to_fs)
                {
                    devfs_add_device_under_node(device_node, node);
                }
                return device_node;
            }
            else
            {
                //device exists!
                return NULL;
            }
        }

        node = n;
    }
    
    return NULL;
}

void devfs_unregister_device(FileSystemNode* device_node)
{
    if (device_node->parent)
    {
        FileSystemNode *parent = device_node->parent;

        if (parent->first_child == device_node)
        {
            parent->first_child = device_node->next_sibling;
        }
        else
        {
            FileSystemNode *n = parent->first_child;
            while (n && n->next_sibling)
            {
                if (n->next_sibling == device_node)
                {
                    n->next_sibling = device_node->next_sibling;
                    break;
                }
                n = n->next_sibling;
            }
        }
    }

    kfree(device_node);
}