#include "fs.h"
#include "common.h"
#include "list.h"
#include "alloc.h"
#include "spinlock.h"
#include "vmm.h"
#include "serial.h"
#include "sharedmemory.h"

static List* g_shm_list = NULL;
//static Spinlock g_shm_list_lock;

static FileSystemNode* g_shm_root = NULL;

static FileSystemDirent g_dirent;

static BOOL sharedmemorydir_open(File *file, uint32_t flags);
static FileSystemNode *sharedmemorydir_create(FileSystemNode* node, char *name, uint32_t flags);
static FileSystemDirent *sharedmemorydir_readdir(FileSystemNode *node, uint32_t index);
static FileSystemNode *sharedmemorydir_finddir(FileSystemNode *node, char *name);

typedef struct MapInfo
{
    Process* process;
    uint32_t v_address;
    uint32_t page_count;
} MapInfo;

typedef struct SharedMemory
{
    FileSystemNode* node;
    List* physical_address_list;
    //Spinlock physical_address_list_lock;
    List* mmapped_list;
    BOOL marked_unlink;
    //TODO: permissions
} SharedMemory;

void sharedmemory_destroy(SharedMemory* shared_mem);

void sharedmemory_initialize()
{
    //spinlock_init(&g_shm_list_lock);

    g_shm_list = list_create();

    FileSystemNode * shm_mount_path = fs_get_node("/dev/shm");

    if (NULL == shm_mount_path)
    {
        FileSystemNode* dev_node = fs_get_node("/dev");
        if (fs_mkdir(dev_node, "shm", 0))
        {
            shm_mount_path = fs_get_node("/dev/shm");
        }
    }
    
    if (shm_mount_path)
    {
        g_shm_root = kmalloc(sizeof(FileSystemNode));
        memset((uint8_t*)g_shm_root, 0, sizeof(FileSystemNode));
        
        shm_mount_path->node_type |= FT_MOUNT_POINT;
        shm_mount_path->mount_point = g_shm_root;

        g_shm_root->parent = g_shm_root->parent;
        g_shm_root->node_type = FT_DIRECTORY;
        
        g_shm_root->open = sharedmemorydir_open;
        g_shm_root->create = sharedmemorydir_create;
        g_shm_root->finddir = sharedmemorydir_finddir;
        g_shm_root->readdir = sharedmemorydir_readdir;

        //TODO: when creating and destroying handle nodes through g_shm_root
    }
    else
    {
        WARNING("/dev/shm cannot initialized!!");
    }
}

static BOOL sharedmemorydir_open(File *file, uint32_t flags)
{
    return TRUE;
}

static FileSystemNode *sharedmemorydir_create(FileSystemNode* node, char *name, uint32_t flags)
{
    return sharedmemory_create(name);
}

static FileSystemDirent *sharedmemorydir_readdir(FileSystemNode *node, uint32_t index)
{
    FileSystemDirent* result = NULL;

    int counter = 0;

    //spinlock_lock(&g_shm_list_lock);

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

    //spinlock_unlock(&g_shm_list_lock);

    return result;
}

static FileSystemNode *sharedmemorydir_finddir(FileSystemNode *node, char *name)
{
    FileSystemNode* result = NULL;

    //spinlock_lock(&g_shm_list_lock);

    list_foreach (n, g_shm_list)
    {
        SharedMemory* p = (SharedMemory*)n->data;

        if (strcmp(name, p->node->name) == 0)
        {
            result = p->node;
            break;
        }
    }

    //spinlock_unlock(&g_shm_list_lock);

    return result;
}

static BOOL sharedmemory_open(File *file, uint32_t flags)
{
    return TRUE;
}

static void sharedmemory_destroy_if_suitable(SharedMemory* shared_memory)
{
    if (shared_memory->marked_unlink && list_get_count(shared_memory->mmapped_list) == 0)
    {
        //printkf("DESTORYING sharedmem (pid:%d)\n", g_current_thread->owner->pid);

        list_foreach (e, shared_memory->physical_address_list)
        {
            uint32_t p_address = (uint32_t)e->data;

            vmm_release_page_frame_4k(p_address);
        }

        sharedmemory_destroy(shared_memory);
    }
}

static int32_t sharedmemory_unlink(FileSystemNode* node, uint32_t flags)
{
    SharedMemory* shared_mem = (SharedMemory*)node->private_node_data;

    //printkf("sharedmemory_unlink(): (pid:%d)\n", g_current_thread->owner->pid);

    shared_mem->marked_unlink = TRUE;

    sharedmemory_destroy_if_suitable(shared_mem);

    return 0;
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

    //spinlock_lock(&shared_mem->physical_address_list_lock);

    for (int i = 0; i < page_count; ++i)
    {
        uint32_t p_address = vmm_acquire_page_frame_4k();

        list_append(shared_mem->physical_address_list, (void*)p_address);
    }

    file->node->length = length;

    //spinlock_unlock(&shared_mem->physical_address_list_lock);

    return 0;
}

static void* sharedmemory_mmap(File* file, uint32_t size, uint32_t offset, uint32_t flags)
{
    void* result = NULL;

    SharedMemory* shared_mem = (SharedMemory*)file->node->private_node_data;

    //spinlock_lock(&shared_mem->physical_address_list_lock);

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

        MapInfo* info = (MapInfo*)kmalloc(sizeof(MapInfo));
        memset((uint8_t*)info, 0, sizeof(MapInfo));
        info->process = g_current_thread->owner;
        info->v_address = (uint32_t)result;
        info->page_count = count;

        list_append(shared_mem->mmapped_list, info);

        kfree(physical_address_array);
    }

    //spinlock_unlock(&shared_mem->physical_address_list_lock);

    return result;
}

BOOL sharedmemory_unmap_if_exists(Process* process, uint32_t address)
{
    list_foreach (n, g_shm_list)
    {
        SharedMemory* p = (SharedMemory*)n->data;

        list_foreach (e, p->mmapped_list)
        {
            MapInfo* info = (MapInfo*)e->data;

            if (info->process == process)
            {
                if (info->v_address == address)
                {
                    list_remove_first_occurrence(p->mmapped_list, info);

                    kfree(info);

                    sharedmemory_destroy_if_suitable(p);

                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}

void sharedmemory_unmap_for_process_all(Process* process)
{
    List* process_shared_mapped_list = list_create();

    list_foreach (n, g_shm_list)
    {
        SharedMemory* p = (SharedMemory*)n->data;

        list_foreach (e, p->mmapped_list)
        {
            MapInfo* info = (MapInfo*)e->data;

            if (info->process == process)
            {
                list_append(process_shared_mapped_list, (void*)info->v_address);
            }
        }
    }

    list_foreach (n, process_shared_mapped_list)
    {
        uint32_t address = (uint32_t)n->data;

        sharedmemory_unmap_if_exists(process, address);
    }

    list_destroy(process_shared_mapped_list);
}

FileSystemNode* sharedmemory_get_node(const char* name)
{
    FileSystemNode* result = NULL;

    //spinlock_lock(&g_shm_list_lock);

    list_foreach (n, g_shm_list)
    {
        SharedMemory* p = (SharedMemory*)n->data;

        if (strcmp(name, p->node->name) == 0)
        {
            result = p->node;
            break;
        }
    }

    //spinlock_unlock(&g_shm_list_lock);

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
    node->unlink = sharedmemory_unlink;
    node->ftruncate = sharedmemory_ftruncate;
    node->mmap = sharedmemory_mmap;
    node->private_node_data = shared_mem;

    shared_mem->node = node;
    shared_mem->physical_address_list = list_create();
    //spinlock_init(&shared_mem->physical_address_list_lock);

    shared_mem->mmapped_list = list_create();

    //spinlock_lock(&g_shm_list_lock);
    list_append(g_shm_list, shared_mem);
    //spinlock_unlock(&g_shm_list_lock);

    return node;
}

SharedMemory* sharedmemory_find(const char* name)
{
    list_foreach (n, g_shm_list)
    {
        SharedMemory* p = (SharedMemory*)n->data;

        if (strcmp(name, p->node->name) == 0)
        {
            return (SharedMemory*)p;
        }
    }

    return NULL;
}

void sharedmemory_destroy(SharedMemory* shared_mem)
{
    //spinlock_lock(&g_shm_list_lock);

    //spinlock_lock(&shared_mem->physical_address_list_lock);

    kfree(shared_mem->node);

    list_destroy(shared_mem->physical_address_list);

    list_destroy(shared_mem->mmapped_list);

    list_remove_first_occurrence(g_shm_list, shared_mem);

    //spinlock_unlock(&shared_mem->physical_address_list_lock);

    kfree(shared_mem);

    //spinlock_unlock(&g_shm_list_lock);
}

BOOL sharedmemory_destroy_by_name(const char* name)
{
    SharedMemory* shared_mem = sharedmemory_find(name);

    if (shared_mem)
    {
        sharedmemory_destroy(shared_mem);

        return TRUE;
    }

    return FALSE;
}
