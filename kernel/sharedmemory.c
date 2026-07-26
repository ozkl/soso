/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#include "fs.h"
#include "common.h"
#include "list.h"
#include "alloc.h"
#include "spinlock.h"
#include "vmm.h"
#include "serial.h"
#include "sharedmemory.h"
#include "errno.h"

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
    List* mmapped_list;
    BOOL marked_unlink;
    BOOL anonymous;
    BOOL allow_sealing;
    uint32_t seals;
    uint32_t open_count;
} SharedMemory;

static void sharedmemory_destroy(SharedMemory* shared_mem);
static FileSystemNode* sharedmemory_create_internal(const char* name, BOOL anonymous, BOOL allow_sealing);

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
        if (p->marked_unlink || p->anonymous)
        {
            continue;
        }

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

        if (!p->marked_unlink &&
            !p->anonymous &&
            strcmp(name, p->node->name) == 0)
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
    SharedMemory* shared_mem = (SharedMemory*)file->node->private_node_data;
    ++shared_mem->open_count;

    return TRUE;
}

static void sharedmemory_destroy_if_suitable(SharedMemory* shared_memory)
{
    if (shared_memory->marked_unlink &&
        shared_memory->open_count == 0 &&
        list_is_empty(shared_memory->mmapped_list))
    {
        sharedmemory_destroy(shared_memory);
    }
}

static void sharedmemory_close(File *file)
{
    SharedMemory* shared_mem = (SharedMemory*)file->node->private_node_data;

    if (shared_mem->open_count > 0)
    {
        --shared_mem->open_count;
    }

    sharedmemory_destroy_if_suitable(shared_mem);
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
    if (length < 0)
    {
        return -EINVAL;
    }

    SharedMemory* shared_mem = (SharedMemory*)file->node->private_node_data;
    uint32_t old_length = file->node->length;

    if ((uint32_t)length < old_length && (shared_mem->seals & SHM_SEAL_SHRINK))
    {
        return -EPERM;
    }

    if ((uint32_t)length > old_length && (shared_mem->seals & SHM_SEAL_GROW))
    {
        return -EPERM;
    }

    uint32_t old_page_count = old_length == 0 ? 0 : PAGE_COUNT(old_length);
    uint32_t new_page_count = length == 0 ? 0 : PAGE_COUNT((uint32_t)length);

    if (new_page_count < old_page_count && !list_is_empty(shared_mem->mmapped_list))
    {
        return -EBUSY;
    }

    if (new_page_count > old_page_count)
    {
        uint32_t added_page_count = new_page_count - old_page_count;

        for (uint32_t i = 0; i < added_page_count; ++i)
        {
            uint32_t physical_address = vmm_acquire_page_frame_4k();
            list_append(shared_mem->physical_address_list, (void*)physical_address);
        }
    }

    if ((uint32_t)length > old_length)
    {
        uint32_t first_page = old_length / PAGESIZE_4K;
        uint32_t clear_page_count = new_page_count - first_page;
        uint32_t* physical_addresses = kmalloc(clear_page_count * sizeof(uint32_t));
        uint32_t source_index = 0;
        uint32_t target_index = 0;
        list_foreach (n, shared_mem->physical_address_list)
        {
            if (source_index >= first_page && target_index < clear_page_count)
            {
                physical_addresses[target_index++] = (uint32_t)n->data;
            }
            ++source_index;
        }

        Process* process = thread_get_current()->owner;
        void* mapped = vmm_map_memory_no_clear(process, USER_MMAP_START, physical_addresses, clear_page_count, FALSE);
        if (mapped == NULL)
        {
            uint32_t added_page_count = new_page_count - old_page_count;
            for (uint32_t i = 0; i < added_page_count; ++i)
            {
                ListNode* node = list_get_last_node(shared_mem->physical_address_list);
                vmm_release_page_frame_4k((uint32_t)node->data);
                list_remove_node(shared_mem->physical_address_list, node);
            }

            kfree(physical_addresses);
            return -ENOMEM;
        }

        uint32_t clear_offset = old_length & (PAGESIZE_4K - 1);
        memset((uint8_t*)mapped + clear_offset, 0, clear_page_count * PAGESIZE_4K - clear_offset);
        vmm_unmap_memory(process, (uint32_t)mapped, clear_page_count);
        kfree(physical_addresses);
    }

    if (new_page_count < old_page_count)
    {
        uint32_t removed_page_count = old_page_count - new_page_count;
        for (uint32_t i = 0; i < removed_page_count; ++i)
        {
            ListNode* node = list_get_last_node(shared_mem->physical_address_list);
            vmm_release_page_frame_4k((uint32_t)node->data);
            list_remove_node(shared_mem->physical_address_list, node);
        }
    }

    file->node->length = length;

    return 0;
}

static void* sharedmemory_mmap(File* file, uint32_t size, uint32_t offset, uint32_t flags)
{
    SharedMemory* shared_mem = (SharedMemory*)file->node->private_node_data;

    if (size == 0 ||
        (flags & MAP_PRIVATE) ||
        (offset & (PAGESIZE_4K - 1)) ||
        offset >= file->node->length)
    {
        return NULL;
    }

    uint32_t first_page = offset / PAGESIZE_4K;
    uint32_t page_count = PAGE_COUNT(size);
    uint32_t physical_page_count = (uint32_t)list_get_count(shared_mem->physical_address_list);
    if (first_page + page_count > physical_page_count)
    {
        return NULL;
    }

    uint32_t* physical_address_array = kmalloc(page_count * sizeof(uint32_t));
    uint32_t source_index = 0;
    uint32_t target_index = 0;
    list_foreach(n, shared_mem->physical_address_list)
    {
        if (source_index >= first_page && target_index < page_count)
        {
            physical_address_array[target_index++] = (uint32_t)n->data;
        }

        ++source_index;
    }

    void* result = vmm_map_memory_no_clear(thread_get_current()->owner, USER_MMAP_START, physical_address_array, page_count, FALSE);
    kfree(physical_address_array);

    if (result != NULL)
    {
        MapInfo* info = (MapInfo*)kmalloc(sizeof(MapInfo));
        memset((uint8_t*)info, 0, sizeof(MapInfo));
        info->process = g_current_thread->owner;
        info->v_address = (uint32_t)result;
        info->page_count = page_count;

        list_append(shared_mem->mmapped_list, info);
    }

    return result;
}

int32_t sharedmemory_unmap_if_exists(Process* process, uint32_t address, uint32_t page_count)
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
                    if (info->page_count != page_count)
                    {
                        return -EINVAL;
                    }

                    list_remove_first_occurrence(p->mmapped_list, info);

                    kfree(info);

                    sharedmemory_destroy_if_suitable(p);

                    return 1;
                }
            }
        }
    }

    return 0;
}

void sharedmemory_unmap_for_process_all(Process* process)
{
    typedef struct PendingUnmap
    {
        uint32_t address;
        uint32_t page_count;
    } PendingUnmap;

    List* pending_unmaps = list_create();

    list_foreach (n, g_shm_list)
    {
        SharedMemory* p = (SharedMemory*)n->data;

        list_foreach (e, p->mmapped_list)
        {
            MapInfo* info = (MapInfo*)e->data;

            if (info->process == process)
            {
                PendingUnmap* pending = kmalloc(sizeof(PendingUnmap));
                pending->address = info->v_address;
                pending->page_count = info->page_count;
                list_append(pending_unmaps, pending);
            }
        }
    }

    list_foreach (n, pending_unmaps)
    {
        PendingUnmap* pending = (PendingUnmap*)n->data;

        sharedmemory_unmap_if_exists(process, pending->address, pending->page_count);
        kfree(pending);
    }

    list_destroy(pending_unmaps);
}

FileSystemNode* sharedmemory_get_node(const char* name)
{
    FileSystemNode* result = NULL;

    //spinlock_lock(&g_shm_list_lock);

    list_foreach (n, g_shm_list)
    {
        SharedMemory* p = (SharedMemory*)n->data;

        if (!p->marked_unlink &&
            !p->anonymous &&
            strcmp(name, p->node->name) == 0)
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
    return sharedmemory_create_internal(name, FALSE, FALSE);
}

FileSystemNode* sharedmemory_create_anonymous(const char* name, BOOL allow_sealing)
{
    return sharedmemory_create_internal(name, TRUE, allow_sealing);
}

static FileSystemNode* sharedmemory_create_internal(const char* name, BOOL anonymous, BOOL allow_sealing)
{
    if (name == NULL || strlen(name) >= sizeof(((FileSystemNode*)0)->name))
    {
        return NULL;
    }

    if (!anonymous && sharedmemory_get_node(name) != NULL)
    {
        return NULL;
    }

    SharedMemory* shared_mem = (SharedMemory*)kmalloc(sizeof(SharedMemory));
    memset((uint8_t*)shared_mem, 0, sizeof(SharedMemory));

    FileSystemNode* node = (FileSystemNode*)kmalloc(sizeof(FileSystemNode));
    memset((uint8_t*)node, 0, sizeof(FileSystemNode));

    strncpy_null(node->name, name, sizeof(node->name));
    node->node_type = FT_FILE;
    node->open = sharedmemory_open;
    node->close = sharedmemory_close;
    node->unlink = sharedmemory_unlink;
    node->ftruncate = sharedmemory_ftruncate;
    node->mmap = sharedmemory_mmap;
    node->private_node_data = shared_mem;

    shared_mem->node = node;
    shared_mem->anonymous = anonymous;
    shared_mem->marked_unlink = anonymous;
    shared_mem->allow_sealing = allow_sealing;
    shared_mem->seals = allow_sealing ? 0 : SHM_SEAL_SEAL;
    shared_mem->physical_address_list = list_create();

    shared_mem->mmapped_list = list_create();

    //spinlock_lock(&g_shm_list_lock);
    list_append(g_shm_list, shared_mem);
    //spinlock_unlock(&g_shm_list_lock);

    return node;
}

static SharedMemory* sharedmemory_from_file(File* file)
{
    if (file == NULL)
    {
        return NULL;
    }

    list_foreach (n, g_shm_list)
    {
        SharedMemory* shared_mem = (SharedMemory*)n->data;
        if (shared_mem->node == file->node)
        {
            return shared_mem;
        }
    }

    return NULL;
}

int32_t sharedmemory_add_seals(File* file, uint32_t seals)
{
    SharedMemory* shared_mem = sharedmemory_from_file(file);
    if (shared_mem == NULL)
    {
        return -EINVAL;
    }

    if (!shared_mem->anonymous)
    {
        return -EINVAL;
    }

    if (!shared_mem->allow_sealing || (shared_mem->seals & SHM_SEAL_SEAL))
    {
        return -EPERM;
    }

    if (seals & ~(SHM_SEAL_SEAL | SHM_SEAL_SHRINK | SHM_SEAL_GROW))
    {
        return -EINVAL;
    }

    shared_mem->seals |= seals;
    return 0;
}

int32_t sharedmemory_get_seals(File* file)
{
    SharedMemory* shared_mem = sharedmemory_from_file(file);
    if (shared_mem == NULL)
    {
        return -EINVAL;
    }

    if (!shared_mem->anonymous)
    {
        return -EINVAL;
    }

    return (int32_t)shared_mem->seals;
}

static SharedMemory* sharedmemory_find(const char* name)
{
    list_foreach (n, g_shm_list)
    {
        SharedMemory* p = (SharedMemory*)n->data;

        if (!p->marked_unlink &&
            !p->anonymous &&
            strcmp(name, p->node->name) == 0)
        {
            return (SharedMemory*)p;
        }
    }

    return NULL;
}

static void sharedmemory_destroy(SharedMemory* shared_mem)
{
    //spinlock_lock(&g_shm_list_lock);

    //spinlock_lock(&shared_mem->physical_address_list_lock);

    list_foreach (n, shared_mem->physical_address_list)
    {
        vmm_release_page_frame_4k((uint32_t)n->data);
    }

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
        shared_mem->marked_unlink = TRUE;
        sharedmemory_destroy_if_suitable(shared_mem);

        return TRUE;
    }

    return FALSE;
}
