#include "fs.h"
#include "alloc.h"
#include "rootfs.h"

FileSystemNode *g_fs_root = NULL; // The root of the filesystem.

#define FILESYSTEM_CAPACITY 10

static FileSystem g_registered_filesystems[FILESYSTEM_CAPACITY];
static int g_next_filesystem_index = 0;

void fs_initialize()
{
    memset((uint8_t*)g_registered_filesystems, 0, sizeof(g_registered_filesystems));

    g_fs_root = rootfs_initialize();

    fs_mkdir(g_fs_root, "dev", 0);
    fs_mkdir(g_fs_root, "initrd", 0);
}

FileSystemNode* fs_get_root_node()
{
    return g_fs_root;
}

int fs_get_node_path(FileSystemNode *node, char* buffer, uint32_t buffer_size)
{
    if (node == g_fs_root)
    {
        if (buffer_size > 1)
        {
            buffer[0] = '/';
            buffer[1] = '\0';

            return 1;
        }
        else
        {
            return -1;
        }
    }

    char target_path[128];

    FileSystemNode *n = node;
    int char_index = 127;
    target_path[char_index] = '\0';
    while (NULL != n)
    {
        int length = strlen(n->name);
        char_index -= length;

        if (char_index < 2)
        {
            return -1;
        }

        if (NULL != n->parent)
        {
            strcpy_nonnull(target_path + char_index, n->name);
            char_index -= 1;
            target_path[char_index] = '/';
        }

        n = n->parent;
    }

    int len = 127 - char_index;

    //Screen_PrintF("fs_get_node_path: len:[%s] %d\n", target_path + char_index, len);

    if (buffer_size < len)
    {
        return -1;
    }

    strcpy(buffer, target_path + char_index);

    return len;
}

BOOL fs_resolve_path(const char* path, char* buffer, int buffer_size)
{
    int length_path = strlen(path);

    if (path[0] != '/')
    {
        return FALSE;
    }

    if (buffer_size < 2)
    {
        return FALSE;
    }

    buffer[0] = '/';
    buffer[1] = '\0';
    int index = 0;
    int index_buffer = 1;

    while (index < length_path - 1)
    {
        while (path[++index] == '/');//eliminate successive

        const char* current = path + index;
        int next_index = str_first_index_of(path + index, '/');

        int length_token = 0;

        if (next_index >= 0)
        {
            const char* next = path + index + next_index;

            length_token = next - (path + index);
        }
        else
        {
            length_token = strlen(current);
        }

        if (length_token > 0)
        {
            index += length_token;
            if (strncmp(current, "..", 2) == 0)
            {
                --index_buffer;
                while (index_buffer > 0)
                {
                    --index_buffer;

                    if (buffer[index_buffer] == '/')
                    {
                        break;
                    }

                    buffer[index_buffer] = '\0';
                }

                ++index_buffer;
                continue;
            }
            else if (strncmp(current, ".", 1) == 0)
            {
                continue;
            }

            if (index_buffer + length_token + 2 > buffer_size)
            {
                return FALSE;
            }

            strncpy(buffer + index_buffer, current, length_token);
            index_buffer += length_token;

            if (current[length_token] == '/')
            {
                buffer[index_buffer++] = '/';
            }
            buffer[index_buffer] = '\0';
        }
    }

    if (index_buffer > 2)
    {
        if (buffer[index_buffer - 1] == '/')
        {
            buffer[index_buffer - 1] = '\0';
        }
    }

    return TRUE;
}

uint32_t fs_read(File *file, uint32_t size, uint8_t *buffer)
{
    if (file->node->read != 0)
    {
        return file->node->read(file, size, buffer);
    }

    return -1;
}

uint32_t fs_write(File *file, uint32_t size, uint8_t *buffer)
{
    if (file->node->write != 0)
    {
        return file->node->write(file, size, buffer);
    }

    return -1;
}

File *fs_open(FileSystemNode *node, uint32_t flags)
{
    return fs_open_for_process(thread_get_current(), node, flags);
}

File *fs_open_for_process(Thread* thread, FileSystemNode *node, uint32_t flags)
{
    Process* process = NULL;
    if (thread)
    {
        process = thread->owner;
    }

    if ( (node->node_type & FT_MOUNT_POINT) == FT_MOUNT_POINT && node->mount_point != NULL )
    {
        node = node->mount_point;
    }

    if (node->open != NULL)
    {
        File* file = kmalloc(sizeof(File));
        memset((uint8_t*)file, 0, sizeof(File));
        file->node = node;
        file->process = process;
        file->thread = thread;
        file->flags = flags;

        BOOL success = node->open(file, flags);

        if (success)
        {
            //Screen_PrintF("Opened:%s\n", file->node->name);
            int32_t fd = process_add_file(file->process, file);

            if (fd < 0)
            {
                //TODO: sett errno max files opened already
                printkf("Maxfiles opened already!!\n");

                fs_close(file);
                file = NULL;
            }
        }
        else
        {
            kfree(file);
            file = NULL;
        }

        return file;
    }

    return NULL;
}

void fs_close(File *file)
{
    if (file->node->close != NULL)
    {
        file->node->close(file);
    }

    process_remove_file(file->process, file);

    kfree(file);
}

int32_t fs_unlink(FileSystemNode* node, uint32_t flags)
{
    if (node->unlink)
    {
        return node->unlink(node, flags);
    }

    return -1;
}

int32_t fs_ioctl(File *file, int32_t request, void * argp)
{
    if (file->node->ioctl != NULL)
    {
        return file->node->ioctl(file, request, argp);
    }

    return 0;
}

int32_t fs_lseek(File *file, int32_t offset, int32_t whence)
{
    if (file->node->lseek != NULL)
    {
        return file->node->lseek(file, offset, whence);
    }

    return 0;
}

int32_t fs_ftruncate(File* file, int32_t length)
{
    if (file->node->ftruncate != NULL)
    {
        return file->node->ftruncate(file, length);
    }

    return -1;
}

int32_t fs_stat(FileSystemNode *node, struct stat *buf)
{
#define	__S_IFDIR	0040000	/* Directory.  */
#define	__S_IFCHR	0020000	/* Character device.  */
#define	__S_IFBLK	0060000	/* Block device.  */
#define	__S_IFREG	0100000	/* Regular file.  */
#define	__S_IFIFO	0010000	/* FIFO.  */
#define	__S_IFLNK	0120000	/* Symbolic link.  */
#define	__S_IFSOCK	0140000	/* Socket.  */

    if (node->stat != NULL)
    {
        int32_t val = node->stat(node, buf);

        if (val == 1)
        {
            //return value of 1 from driver means we should fill buf here.

            if ((node->node_type & FT_DIRECTORY) == FT_DIRECTORY)
            {
                buf->st_mode = __S_IFDIR;
            }
            else if ((node->node_type & FT_CHARACTER_DEVICE) == FT_CHARACTER_DEVICE)
            {
                buf->st_mode = __S_IFCHR;
            }
            else if ((node->node_type & FT_BLOCK_DEVICE) == FT_BLOCK_DEVICE)
            {
                buf->st_mode = __S_IFBLK;
            }
            else if ((node->node_type & FT_PIPE) == FT_PIPE)
            {
                buf->st_mode = __S_IFIFO;
            }
            else if ((node->node_type & FT_SYMBOLIC_LINK) == FT_SYMBOLIC_LINK)
            {
                buf->st_mode = __S_IFLNK;
            }
            else if ((node->node_type & FT_FILE) == FT_FILE)
            {
                buf->st_mode = __S_IFREG;
            }

            buf->st_size = node->length;

            return 0;
        }
        else
        {
            return val;
        }
    }

    return -1;
}

FileSystemDirent *fs_readdir(FileSystemNode *node, uint32_t index)
{
    //Screen_PrintF("fs_readdir: node->name:%s index:%d\n", node->name, index);

    if ( (node->node_type & FT_MOUNT_POINT) == FT_MOUNT_POINT && node->mount_point != NULL )
    {
        if (NULL == node->mount_point->readdir)
        {
            WARNING("mounted fs does not have readdir!\n");
        }
        else
        {
            return node->mount_point->readdir(node->mount_point, index);
        }
    }
    else if ( (node->node_type & FT_DIRECTORY) == FT_DIRECTORY && node->readdir != NULL )
    {
        return node->readdir(node, index);
    }

    return NULL;
}

FileSystemNode *fs_finddir(FileSystemNode *node, char *name)
{
    //Screen_PrintF("fs_finddir: name:%s\n", name);

    if ( (node->node_type & FT_MOUNT_POINT) == FT_MOUNT_POINT && node->mount_point != NULL )
    {
        if (NULL == node->mount_point->finddir)
        {
            WARNING("mounted fs does not have finddir!\n");
        }
        else
        {
            return node->mount_point->finddir(node->mount_point, name);
        }
    }
    else if ( (node->node_type & FT_DIRECTORY) == FT_DIRECTORY && node->finddir != NULL )
    {
        return node->finddir(node, name);
    }

    return NULL;
}

BOOL fs_mkdir(FileSystemNode *node, const char *name, uint32_t flags)
{
    if ( (node->node_type & FT_MOUNT_POINT) == FT_MOUNT_POINT && node->mount_point != NULL )
    {
        if (node->mount_point->mkdir)
        {
            return node->mount_point->mkdir(node->mount_point, name, flags);
        }
    }
    else if ( (node->node_type & FT_DIRECTORY) == FT_DIRECTORY && node->mkdir != NULL )
    {
        return node->mkdir(node, name, flags);
    }

    return FALSE;
}

void* fs_mmap(File* file, uint32_t size, uint32_t offset, uint32_t flags)
{
    if (file->node->mmap)
    {
        return file->node->mmap(file, size, offset, flags);
    }

    return NULL;
}

BOOL fs_munmap(File* file, void* address, uint32_t size)
{
    if (file->node->munmap)
    {
        return file->node->munmap(file, address, size);
    }

    return FALSE;
}

FileSystemNode *fs_get_node(const char *path)
{
    //Screen_PrintF("fs_get_node:%s *0\n", path);

    if (path[0] != '/')
    {
        //We require absolute path!
        return NULL;
    }


    char real_path[256];

    BOOL resolved = fs_resolve_path(path, real_path, 256);

    if (FALSE == resolved)
    {
        return NULL;
    }

    const char* input_path = real_path;
    int path_length = strlen(input_path);

    if (path_length < 1)
    {
        return NULL;
    }



    //Screen_PrintF("fs_get_node:%s *1\n", path);

    FileSystemNode* root = fs_get_root_node();

    if (path_length == 1)
    {
        return root;
    }

    int next_index = 0;

    FileSystemNode* node = root;

    //Screen_PrintF("fs_get_node:%s *2\n", path);

    char buffer[64];

    do
    {
        do_start:
        input_path = input_path + next_index + 1;
        next_index = str_first_index_of(input_path, '/');

        if (next_index == 0)
        {
            //detected successive slash
            goto do_start;
        }

        if (next_index > 0)
        {
            int token_size = next_index;

            strncpy(buffer, input_path, token_size);
            buffer[token_size] = '\0';
        }
        else
        {
            //Last part
            strcpy(buffer, input_path);
        }

        //Screen_PrintF("fs_get_node:%s *3\n", path);

        node = fs_finddir(node, buffer);

        //Screen_PrintF("fs_get_node:%s *4\n", path);

        if (NULL == node)
        {
            return NULL;
        }

    } while (next_index > 0);

    return node;
}

FileSystemNode* fs_get_node_absolute_or_relative(const char* path, Process* process)
{
    FileSystemNode* node = NULL;

    if (process)
    {
        if ('\0' == path[0])
        {
            //empty
        }
        else if ('/' == path[0])
        {
            //absolute

            node = fs_get_node(path);
        }
        else
        {
            //relative

            if (process->working_directory)
            {
                node = fs_get_node_relative_to_node(path, process->working_directory);
            }
        }
    }

    return node;
}

FileSystemNode* fs_get_node_relative_to_node(const char* path, FileSystemNode* relative_to)
{
    FileSystemNode* node = NULL;

    if (relative_to)
    {
        char buffer[256];

        if (fs_get_node_path(relative_to, buffer, 256) >= 0)
        {
            strcat(buffer, "/");
            strcat(buffer, path);

            node = fs_get_node(buffer);
        }
    }

    return node;
}

BOOL fs_register(FileSystem* fs)
{
    if (strlen(fs->name) <= 0)
    {
        return FALSE;
    }

    for (int i = 0; i < g_next_filesystem_index; ++i)
    {
        if (strcmp(g_registered_filesystems[i].name, fs->name) == 0)
        {
            //name is in use
            return FALSE;
        }
    }

    g_registered_filesystems[g_next_filesystem_index++] = *fs;

    return TRUE;
}

BOOL fs_mount(const char *source, const char *target, const char *fsType, uint32_t flags, void *data)
{
    FileSystem* fs = NULL;

    for (int i = 0; i < g_next_filesystem_index; ++i)
    {
        if (strcmp(g_registered_filesystems[i].name, fsType) == 0)
        {
            fs = &g_registered_filesystems[i];
            break;
        }
    }

    if (NULL == fs)
    {
        return FALSE;
    }

    return fs->mount(source, target, flags, data);
}

BOOL fs_check_mount(const char *source, const char *target, const char *fsType, uint32_t flags, void *data)
{
    FileSystem* fs = NULL;

    for (int i = 0; i < g_next_filesystem_index; ++i)
    {
        if (strcmp(g_registered_filesystems[i].name, fsType) == 0)
        {
            fs = &g_registered_filesystems[i];
            break;
        }
    }

    if (NULL == fs)
    {
        return FALSE;
    }

    return fs->check_mount(source, target, flags, data);
}
