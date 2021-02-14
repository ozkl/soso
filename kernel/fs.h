#ifndef FS_H
#define FS_H

#include "common.h"

#define O_ACCMODE   0003
#define O_RDONLY    00
#define O_WRONLY    01
#define O_RDWR      02
#define CHECK_ACCESS(flags, test) ((flags & O_ACCMODE) == test)

typedef enum FileType
{
    FT_FILE               = 1,
    FT_CHARACTER_DEVICE   = 2,
    FT_BLOCK_DEVICE       = 3,
    FT_PIPE               = 4,
    FT_SYMBOLIC_LINK      = 5,
    FT_SOCKET             = 6,
    FT_DIRECTORY          = 128,
    FT_MOUNT_POINT        = 256
} FileType;

typedef enum IoctlCommand
{
    IC_GET_SECTOR_SIZE_BYTES,
    IC_GET_SECTOR_COUNT,
} IoctlCommand;

typedef struct FileSystem FileSystem;
typedef struct FileSystemNode FileSystemNode;
typedef struct FileSystemDirent FileSystemDirent;
typedef struct Process Process;
typedef struct Thread Thread;
typedef struct File File;

struct stat;

typedef int32_t (*ReadWriteFunction)(File* file, uint32_t size, uint8_t* buffer);
typedef BOOL (*ReadWriteTestFunction)(File* file);
typedef int32_t (*ReadWriteBlockFunction)(FileSystemNode* node, uint32_t block_number, uint32_t count, uint8_t* buffer);
typedef BOOL (*OpenFunction)(File* file, uint32_t flags);
typedef void (*CloseFunction)(File* file);
typedef int32_t (*UnlinkFunction)(FileSystemNode* node, uint32_t flags);
typedef int32_t (*IoctlFunction)(File *file, int32_t request, void * argp);
typedef int32_t (*LseekFunction)(File *file, int32_t offset, int32_t whence);
typedef int32_t (*FtruncateFunction)(File *file, int32_t length);
typedef int32_t (*StatFunction)(FileSystemNode *node, struct stat *buf);
typedef FileSystemDirent * (*ReadDirFunction)(FileSystemNode*,uint32_t);
typedef FileSystemNode * (*FindDirFunction)(FileSystemNode*,char *name);
typedef BOOL (*MkDirFunction)(FileSystemNode* node, const char *name, uint32_t flags);
typedef void* (*MmapFunction)(File* file, uint32_t size, uint32_t offset, uint32_t flags);
typedef BOOL (*MunmapFunction)(File* file, void* address, uint32_t size);

typedef BOOL (*MountFunction)(const char* source_path, const char* target_path, uint32_t flags, void *data);

typedef struct FileSystem
{
    char name[32];
    MountFunction check_mount;
    MountFunction mount;
} FileSystem;

typedef struct FileSystemNode
{
    char name[128];
    uint32_t mask;
    uint32_t user_id;
    uint32_t group_id;
    uint32_t node_type;
    uint32_t inode;
    uint32_t length;
    ReadWriteBlockFunction read_block;
    ReadWriteBlockFunction write_block;
    ReadWriteFunction read;
    ReadWriteFunction write;
    ReadWriteTestFunction read_test_ready;
    ReadWriteTestFunction write_test_ready;
    OpenFunction open;
    CloseFunction close;
    UnlinkFunction unlink;
    IoctlFunction ioctl;
    LseekFunction lseek;
    FtruncateFunction ftruncate;
    StatFunction stat;
    ReadDirFunction readdir;
    FindDirFunction finddir;
    MkDirFunction mkdir;
    MmapFunction mmap;
    MunmapFunction munmap;
    FileSystemNode *first_child;
    FileSystemNode *next_sibling;
    FileSystemNode *parent;
    FileSystemNode *mount_point;//only used in mounts
    FileSystemNode *mount_source;//only used in mounts
    void* private_node_data;
} FileSystemNode;

typedef struct FileSystemDirent
{
    char name[128];
    FileType file_type;
    uint32_t inode;
} FileSystemDirent;

//Per open
typedef struct File
{
    FileSystemNode* node;
    Process* process;
    Thread* thread;
    int32_t fd;
    uint32_t flags;
    int32_t offset;
    void* private_data;
} File;

struct stat
{
    uint16_t/*dev_t      */ st_dev;     /* ID of device containing file */
    uint16_t/*ino_t      */ st_ino;     /* inode number */
    uint32_t/*mode_t     */ st_mode;    /* protection */
    uint16_t/*nlink_t    */ st_nlink;   /* number of hard links */
    uint16_t/*uid_t      */ st_uid;     /* user ID of owner */
    uint16_t/*gid_t      */ st_gid;     /* group ID of owner */
    uint16_t/*dev_t      */ st_rdev;    /* device ID (if special file) */
    uint32_t/*off_t      */ st_size;    /* total size, in bytes */

    uint32_t/*time_t     */ st_atime;
    uint32_t/*long       */ st_spare1;
    uint32_t/*time_t     */ st_mtime;
    uint32_t/*long       */ st_spare2;
    uint32_t/*time_t     */ st_ctime;
    uint32_t/*long       */ st_spare3;
    uint32_t/*blksize_t  */ st_blksize;
    uint32_t/*blkcnt_t   */ st_blocks;
    uint32_t/*long       */ st_spare4[2];
};


uint32_t fs_read(File* file, uint32_t size, uint8_t* buffer);
uint32_t fs_write(File* file, uint32_t size, uint8_t* buffer);
File* fs_open(FileSystemNode* node, uint32_t flags);
File* fs_open_for_process(Thread* thread, FileSystemNode* node, uint32_t flags);
void fs_close(File* file);
int32_t fs_unlink(FileSystemNode* node, uint32_t flags);
int32_t fs_ioctl(File* file, int32_t request, void* argp);
int32_t fs_lseek(File* file, int32_t offset, int32_t whence);
int32_t fs_ftruncate(File* file, int32_t length);
int32_t fs_stat(FileSystemNode *node, struct stat *buf);
FileSystemDirent* fs_readdir(FileSystemNode* node, uint32_t index);
FileSystemNode* fs_finddir(FileSystemNode* node, char* name);
BOOL fs_mkdir(FileSystemNode *node, const char* name, uint32_t flags);
void* fs_mmap(File* file, uint32_t size, uint32_t offset, uint32_t flags);
BOOL fs_munmap(File* file, void* address, uint32_t size);
int fs_get_node_path(FileSystemNode* node, char* buffer, uint32_t buffer_size);
BOOL fs_resolve_path(const char* path, char* buffer, int buffer_size);

void fs_initialize();
FileSystemNode* fs_get_root_node();
FileSystemNode* fs_get_node(const char* path);
FileSystemNode* fs_get_node_absolute_or_relative(const char* path, Process* process);
FileSystemNode* fs_get_node_relative_to_node(const char* path, FileSystemNode* relative_to);

BOOL fs_register(FileSystem* fs);
BOOL fs_mount(const char *source, const char *target, const char *fsType, uint32_t flags, void *data);
BOOL fs_check_mount(const char *source, const char *target, const char *fsType, uint32_t flags, void *data);

#endif
