#include "rootfs.h"
#include "alloc.h"

static BOOL rootfs_open(File *node, uint32 flags);
static void rootfs_close(File *file);
static FileSystemNode *rootfs_finddir(FileSystemNode *node, char *name);
static struct FileSystemDirent *rootfs_readdir(FileSystemNode *node, uint32 index);
static BOOL rootfs_mkdir(FileSystemNode *node, const char *name, uint32 flags);

FileSystemNode* rootfs_initialize()
{
    FileSystemNode* root = (FileSystemNode*)kmalloc(sizeof(FileSystemNode));
    memset((uint8*)root, 0, sizeof(FileSystemNode));
    root->nodeType = FT_Directory;
    root->open = rootfs_open;
    root->close = rootfs_close;
    root->readdir = rootfs_readdir;
    root->finddir = rootfs_finddir;
    root->mkdir = rootfs_mkdir;

    return root;
}

static FileSystemDirent g_dirent;

static BOOL rootfs_open(File *node, uint32 flags)
{
    return TRUE;
}

static void rootfs_close(File *file)
{

}

static struct FileSystemDirent *rootfs_readdir(FileSystemNode *node, uint32 index)
{
    FileSystemNode *n = node->firstChild;
    uint32 i = 0;
    while (NULL != n)
    {
        if (index == i)
        {
            g_dirent.fileType = n->nodeType;
            g_dirent.inode = n->inode;
            strcpy(g_dirent.name, n->name);

            return &g_dirent;
        }
        n = n->nextSibling;
        ++i;
    }

    return NULL;
}

static FileSystemNode *rootfs_finddir(FileSystemNode *node, char *name)
{
    FileSystemNode *n = node->firstChild;
    while (NULL != n)
    {
        if (strcmp(name, n->name) == 0)
        {
            return n;
        }
        n = n->nextSibling;
    }

    return NULL;
}

static BOOL rootfs_mkdir(FileSystemNode *node, const char *name, uint32 flags)
{
    FileSystemNode *n = node->firstChild;
    while (NULL != n)
    {
        if (strcmp(name, n->name) == 0)
        {
            return FALSE;
        }
        n = n->nextSibling;
    }

    FileSystemNode* new_node = (FileSystemNode*)kmalloc(sizeof(FileSystemNode));
    memset((uint8*)new_node, 0, sizeof(FileSystemNode));
    strcpy(new_node->name, name);
    new_node->nodeType = FT_Directory;
    new_node->open = rootfs_open;
    new_node->close = rootfs_close;
    new_node->readdir = rootfs_readdir;
    new_node->finddir = rootfs_finddir;
    new_node->mkdir = rootfs_mkdir;
    new_node->parent = node;

    if (node->firstChild == NULL)
    {
        node->firstChild = new_node;
    }
    else
    {
        FileSystemNode *n = node->firstChild;
        while (NULL != n->nextSibling)
        {
            n = n->nextSibling;
        }
        n->nextSibling = new_node;
    }

    return TRUE;
}

