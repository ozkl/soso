#include "fs.h"
#include "common.h"
#include "list.h"
#include "alloc.h"
#include "spinlock.h"
#include "vmm.h"
#include "sharedmemory.h"

static List* g_shm_list = NULL;
static Spinlock g_shm_list_lock;

static FileSystemNode* g_shm_root = NULL;

static FileSystemDirent g_dirent;

static BOOL sharedmemorydir_open(File *file, uint32_t flags);
static FileSystemDirent *sharedmemorydir_readdir(FileSystemNode *node, uint32_t index);
static FileSystemNode *sharedmemorydir_finddir(FileSystemNode *node, char *name);

typedef struct SharedMemory
{
    FileSystemNode* node;
    List* physical_address_list;
    Spinlock physical_address_list_lock;
    //TODO: permissions
} SharedMemory;

void sharedmemory_initialize()
{
    spinlock_init(&g_shm_list_lock);

    g_shm_list = list_create();

    g_shm_root = fs_get_node("/system/shm");

    if (NULL == g_shm_root)
    {
        WARNING("/system/shm not found!!");
    }
    else
    {
        g_shm_root->open = sharedmemorydir_open;
        g_shm_root->finddir = sharedmemorydir_finddir;
        g_shm_root->readdir = sharedmemorydir_readdir;
    }
}

static BOOL sharedmemorydir_open(File *file, uint32_t flags)
{
    return TRUE;
}

static FileSystemDirent *sharedmemorydir_readdir(FileSystemNode *node, uint32_t index)
{
    FileSystemDirent* result = NULL;

    int counter = 0;

    spinlock_lock(&g_shm_list_lock);

    list_foreach (n, g_shm_list)
    {
        SharedMemory* p = (SharedMemory*)n->data;

        if (counter == index)
        {
            strcpy(g_dirent.name, p->node->name);
            g_dirent.file_type = p->node->node_type;

            result = &g_dirent;

            break;
        }
        ++counter;
    }

    spinlock_unlock(&g_shm_list_lock);

    return result;
}

static FileSystemNode *sharedmemorydir_finddir(FileSystemNode *node, char *name)
{
    FileSystemNode* result = NULL;

    spinlock_lock(&g_shm_list_lock);

    list_foreach (n, g_shm_list)
    {
        SharedMemory* p = (SharedMemory*)n->data;

        if (strcmp(name, p->node->name) == 0)
        {
            result = p->node;
            break;
        }
    }

    spinlock_unlock(&g_shm_list_lock);

    return result;
}

static BOOL sharedmemory_open(File *file, uint32_t flags)
{
    return TRUE;
}

static void sharedmemory_unlink(File *file)
{
    sharedmemory_destroy(file->node->name);
}

static int32_t sharedmemory_ftruncate(File *file, int32_t length)
{
    if (length <= 0)
    {
        return -1;
    }

    SharedMemory* shared_mem = (SharedMemory*)file->node->private_node_data;

    if (0 != file->node->length)
    {
        //already set
        return -1;
    }

    int page_count = PAGE_COUNT(length);

    spinlock_lock(&shared_mem->physical_address_list_lock);

    for (int i = 0; i < page_count; ++i)
    {
        //TODO: where should this be released? maybe on destroy?
        uint32_t p_address = vmm_acquire_page_frame_4k();

        list_append(shared_mem->physical_address_list, (void*)p_address);
    }

    file->node->length = length;

    spinlock_unlock(&shared_mem->physical_address_list_lock);

    return 0;
}

static void* sharedmemory_mmap(File* file, uint32_t size, uint32_t offset, uint32_t flags)
{
    void* result = NULL;

    SharedMemory* shared_mem = (SharedMemory*)file->node->private_node_data;

    spinlock_lock(&shared_mem->physical_address_list_lock);

    int count = list_get_count(shared_mem->physical_address_list);
    if (count > 0)
    {
        uint32_t* physical_address_array = (uint32_t*)kmalloc(count * sizeof(uint32_t));
        int i = 0;
        list_foreach(n, shared_mem->physical_address_list)
        {
            physical_address_array[i] = (uint32_t)n->data;

            ++i;
        }
        result = vmm_map_memory(file->thread->owner, USER_MMAP_START, physical_address_array, count, FALSE);

        kfree(physical_address_array);
    }

    spinlock_unlock(&shared_mem->physical_address_list_lock);

    return result;
}

FileSystemNode* sharedmemory_get_node(const char* name)
{
    FileSystemNode* result = NULL;

    spinlock_lock(&g_shm_list_lock);

    list_foreach (n, g_shm_list)
    {
        SharedMemory* p = (SharedMemory*)n->data;

        if (strcmp(name, p->node->name) == 0)
        {
            result = p->node;
            break;
        }
    }

    spinlock_unlock(&g_shm_list_lock);

    return result;
}

FileSystemNode* sharedmemory_create(const char* name)
{
    if (sharedmemory_get_node(name) != NULL)
    {
        return NULL;
    }

    SharedMemory* shared_mem = (SharedMemory*)kmalloc(sizeof(SharedMemory));
    memset((uint8_t*)shared_mem, 0, sizeof(SharedMemory));

    FileSystemNode* node = (FileSystemNode*)kmalloc(sizeof(FileSystemNode));
    memset((uint8_t*)node, 0, sizeof(FileSystemNode));

    strcpy(node->name, name);
    node->node_type = FT_CHARACTER_DEVICE;
    node->open = sharedmemory_open;
    //TODO: node->shm_unlink = sharedmemory_unlink;
    node->ftruncate = sharedmemory_ftruncate;
    node->mmap = sharedmemory_mmap;
    node->private_node_data = shared_mem;

    shared_mem->node = node;
    shared_mem->physical_address_list = list_create();
    spinlock_init(&shared_mem->physical_address_list_lock);

    spinlock_lock(&g_shm_list_lock);
    list_append(g_shm_list, shared_mem);
    spinlock_unlock(&g_shm_list_lock);

    return node;
}

void sharedmemory_destroy(const char* name)
{
    SharedMemory* shared_mem = NULL;

    spinlock_lock(&g_shm_list_lock);

    list_foreach (n, g_shm_list)
    {
        SharedMemory* p = (SharedMemory*)n->data;

        if (strcmp(name, p->node->name) == 0)
        {
            shared_mem = (SharedMemory*)p;
            break;
        }
    }

    if (shared_mem)
    {
        spinlock_lock(&shared_mem->physical_address_list_lock);

        kfree(shared_mem->node);

        list_destroy(shared_mem->physical_address_list);

        list_remove_first_occurrence(g_shm_list, shared_mem);

        spinlock_unlock(&shared_mem->physical_address_list_lock);

        kfree(shared_mem);
    }

    spinlock_unlock(&g_shm_list_lock);
}
