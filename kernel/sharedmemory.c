#include "fs.h"
#include "common.h"
#include "list.h"
#include "alloc.h"
#include "spinlock.h"
#include "vmm.h"
#include "sharedmemory.h"

static List* gShmList = NULL;
static Spinlock gShmListLock;

static FileSystemNode* gShmRoot = NULL;

static FileSystemDirent gDirent;

static BOOL sharedmemorydir_open(File *file, uint32 flags);
static FileSystemDirent *sharedmemorydir_readdir(FileSystemNode *node, uint32 index);
static FileSystemNode *sharedmemorydir_finddir(FileSystemNode *node, char *name);

typedef struct SharedMemory
{
    FileSystemNode* node;
    List* physicalAddressList;
    uint32 size;
} SharedMemory;

void initializeSharedMemory()
{
    Spinlock_Init(&gShmListLock);

    gShmList = List_Create();

    gShmRoot = getFileSystemNode("/system/shm");

    if (NULL == gShmRoot)
    {
        WARNING("/system/shm not found!!");
    }
    else
    {
        gShmRoot->open = sharedmemorydir_open;
        gShmRoot->finddir = sharedmemorydir_finddir;
        gShmRoot->readdir = sharedmemorydir_readdir;
    }
}

static BOOL sharedmemorydir_open(File *file, uint32 flags)
{
    return TRUE;
}

static FileSystemDirent *sharedmemorydir_readdir(FileSystemNode *node, uint32 index)
{
    FileSystemDirent* result = NULL;

    int counter = 0;

    Spinlock_Lock(&gShmListLock);

    List_Foreach (n, gShmList)
    {
        SharedMemory* p = (SharedMemory*)n->data;

        if (counter == index)
        {
            strcpy(gDirent.name, p->node->name);
            gDirent.fileType = p->node->nodeType;

            result = &gDirent;

            break;
        }
        ++counter;
    }

    Spinlock_Unlock(&gShmListLock);

    return result;
}

static FileSystemNode *sharedmemorydir_finddir(FileSystemNode *node, char *name)
{
    FileSystemNode* result = NULL;

    Spinlock_Lock(&gShmListLock);

    List_Foreach (n, gShmList)
    {
        SharedMemory* p = (SharedMemory*)n->data;

        if (strcmp(name, p->node->name) == 0)
        {
            result = p->node;
            break;
        }
    }

    Spinlock_Unlock(&gShmListLock);

    return result;
}

static BOOL sharedmemory_open(File *file, uint32 flags)
{
    return TRUE;
}

static void sharedmemory_close(File *file)
{

}

static int32 sharedmemory_ftruncate(File *file, int32 length)
{
    if (length <= 0)
    {
        return -1;
    }

    SharedMemory* sharedMem = (SharedMemory*)file->node->privateNodeData;

    if (0 != file->node->length)
    {
        //already set
        return -1;
    }

    int pageCount = (length / PAGESIZE_4M) + 1;

    for (int i = 0; i < pageCount; ++i)
    {
        char* pAddress = getPageFrame4M();

        List_Append(sharedMem->physicalAddressList, pAddress);
    }

    file->node->length = length;

    return 0;
}

static void createSharedMemory(const char* name)
{
    SharedMemory* sharedMem = (SharedMemory*)kmalloc(sizeof(SharedMemory));
    memset((uint8*)sharedMem, 0, sizeof(SharedMemory));

    FileSystemNode* node = (FileSystemNode*)kmalloc(sizeof(FileSystemNode));
    memset((uint8*)node, 0, sizeof(FileSystemNode));

    strcpy(node->name, name);
    node->nodeType = FT_CharacterDevice;
    node->open = sharedmemory_open;
    node->close = sharedmemory_close;
    node->ftruncate = sharedmemory_ftruncate;
    node->privateNodeData = sharedMem;

    //TODO: mmap

    sharedMem->node = node;
    sharedMem->physicalAddressList = List_Create();

    Spinlock_Lock(&gShmListLock);
    List_Append(gShmList, sharedMem);
    Spinlock_Unlock(&gShmListLock);
}

static void destroySharedMemory(const char* name)
{
    //TODO
}

int shm_open(const char *name, int oflag, int mode);

int shm_unlink(const char *name);
