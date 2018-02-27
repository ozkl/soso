#include "systemfs.h"
#include "common.h"
#include "fs.h"
#include "alloc.h"
#include "device.h"
#include "screen.h"
#include "vmm.h"

static FileSystemNode* gSystemFsRoot = NULL;


static BOOL systemfs_open(File *node, uint32 flags);
static FileSystemDirent *systemfs_readdir(FileSystemNode *node, uint32 index);
static FileSystemNode *systemfs_finddir(FileSystemNode *node, char *name);

static void createNodes();

static FileSystemDirent gDirent;

static int32 systemfs_read_meminfo_totalpages(File *file, uint32 size, uint8 *buffer);
static int32 systemfs_read_meminfo_usedpages(File *file, uint32 size, uint8 *buffer);
static BOOL systemfs_open_processes(File *node, uint32 flags);
static void systemfs_close_processes(File *file);

void initializeSystemFS()
{
    gSystemFsRoot = kmalloc(sizeof(FileSystemNode));
    memset((uint8*)gSystemFsRoot, 0, sizeof(FileSystemNode));

    gSystemFsRoot->nodeType = FT_Directory;

    FileSystemNode* rootFs = getFileSystemRootNode();

    mkdir_fs(rootFs, "system", 0);

    FileSystemNode* systemNode = finddir_fs(rootFs, "system");

    if (systemNode)
    {
        systemNode->nodeType |= FT_MountPoint;
        systemNode->mountPoint = gSystemFsRoot;
        gSystemFsRoot->parent = systemNode->parent;
        strcpy(gSystemFsRoot->name, systemNode->name);
    }
    else
    {
        PANIC("Could not create /system !");
    }

    gSystemFsRoot->open = systemfs_open;
    gSystemFsRoot->finddir = systemfs_finddir;
    gSystemFsRoot->readdir = systemfs_readdir;

    createNodes();
}

static void createNodes()
{
    FileSystemNode* nodeMemInfo = kmalloc(sizeof(FileSystemNode));

    memset((uint8*)nodeMemInfo, 0, sizeof(FileSystemNode));

    strcpy(nodeMemInfo->name, "meminfo");
    nodeMemInfo->nodeType = FT_Directory;
    nodeMemInfo->open = systemfs_open;
    nodeMemInfo->finddir = systemfs_finddir;
    nodeMemInfo->readdir = systemfs_readdir;
    nodeMemInfo->parent = gSystemFsRoot;

    gSystemFsRoot->firstChild = nodeMemInfo;

    FileSystemNode* nodeMemInfoTotalPages = kmalloc(sizeof(FileSystemNode));
    memset((uint8*)nodeMemInfoTotalPages, 0, sizeof(FileSystemNode));
    strcpy(nodeMemInfoTotalPages->name, "totalpages");
    nodeMemInfoTotalPages->nodeType = FT_File;
    nodeMemInfoTotalPages->open = systemfs_open;
    nodeMemInfoTotalPages->read = systemfs_read_meminfo_totalpages;
    nodeMemInfoTotalPages->parent = nodeMemInfo;

    nodeMemInfo->firstChild = nodeMemInfoTotalPages;

    FileSystemNode* nodeMemInfoUsedPages = kmalloc(sizeof(FileSystemNode));
    memset((uint8*)nodeMemInfoUsedPages, 0, sizeof(FileSystemNode));
    strcpy(nodeMemInfoUsedPages->name, "usedpages");
    nodeMemInfoUsedPages->nodeType = FT_File;
    nodeMemInfoUsedPages->open = systemfs_open;
    nodeMemInfoUsedPages->read = systemfs_read_meminfo_usedpages;
    nodeMemInfoUsedPages->parent = nodeMemInfo;

    nodeMemInfoTotalPages->nextSibling = nodeMemInfoUsedPages;

    //

    FileSystemNode* nodeProcesses = kmalloc(sizeof(FileSystemNode));
    memset((uint8*)nodeProcesses, 0, sizeof(FileSystemNode));

    strcpy(nodeProcesses->name, "processes");
    nodeProcesses->nodeType = FT_Directory;
    nodeProcesses->open = systemfs_open_processes;
    nodeProcesses->close = systemfs_close_processes;
    nodeProcesses->finddir = systemfs_finddir;
    nodeProcesses->readdir = systemfs_readdir;
    nodeProcesses->parent = gSystemFsRoot;

    nodeMemInfo->nextSibling = nodeProcesses;
}

static BOOL systemfs_open(File *node, uint32 flags)
{
    return TRUE;
}

static FileSystemDirent *systemfs_readdir(FileSystemNode *node, uint32 index)
{
    int counter = 0;

    FileSystemNode* child = node->firstChild;

    //Screen_PrintF("systemfs_readdir-main:%s index:%d\n", node->name, index);

    while (NULL != child)
    {
        //Screen_PrintF("systemfs_readdir-child:%s\n", child->name);
        if (counter == index)
        {
            strcpy(gDirent.name, child->name);
            gDirent.fileType = child->nodeType;

            return &gDirent;
        }

        ++counter;

        child = child->nextSibling;
    }

    return NULL;
}

static FileSystemNode *systemfs_finddir(FileSystemNode *node, char *name)
{
    //Screen_PrintF("systemfs_finddir-main:%s requestedName:%s\n", node->name, name);

    FileSystemNode* child = node->firstChild;
    while (NULL != child)
    {
        if (strcmp(name, child->name) == 0)
        {
            //Screen_PrintF("systemfs_finddir-found:%s\n", name);
            return child;
        }

        child = child->nextSibling;
    }

    return NULL;
}

static int32 systemfs_read_meminfo_totalpages(File *file, uint32 size, uint8 *buffer)
{
    if (size >= 4)
    {
        if (file->offset == 0)
        {
            int totalPages = getTotalPageCount();

            sprintf(buffer, "%d", totalPages);

            int len = strlen(buffer);

            file->offset += len;

            return len;
        }
        else
        {
            return 0;
        }
    }
    return -1;
}

static int32 systemfs_read_meminfo_usedpages(File *file, uint32 size, uint8 *buffer)
{
    if (size >= 4)
    {
        if (file->offset == 0)
        {
            int usedPages = getUsedPageCount();

            sprintf(buffer, "%d", usedPages);

            int len = strlen(buffer);

            file->offset += len;

            return len;
        }
        else
        {
            return 0;
        }
    }
    return -1;
}

static BOOL systemfs_open_processes(File *node, uint32 flags)
{
    //TODO

    return TRUE;
}

static void systemfs_close_processes(File *file)
{
    if (file->privateData == NULL)
    {
        return;
    }

    //TODO

    file->privateData = NULL;
}
