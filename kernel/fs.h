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
    FT_File               = 1,
    FT_CharacterDevice    = 2,
    FT_BlockDevice        = 3,
    FT_Pipe               = 4,
    FT_SymbolicLink       = 5,
    FT_Directory          = 128,
    FT_MountPoint         = 256
} FileType;

typedef enum IoctlCommand
{
    IC_GetSectorSizeInBytes,
    IC_GetSectorCount,
} IoctlCommand;

typedef struct FileSystem FileSystem;
typedef struct FileSystemNode FileSystemNode;
typedef struct FileSystemDirent FileSystemDirent;
typedef struct Process Process;
typedef struct Thread Thread;
typedef struct File File;

struct stat;

typedef int32 (*ReadWriteFunction)(File* file, uint32 size, uint8* buffer);
typedef BOOL (*ReadWriteTestFunction)(File* file);
typedef int32 (*ReadWriteBlockFunction)(FileSystemNode* node, uint32 blockNumber, uint32 count, uint8* buffer);
typedef BOOL (*OpenFunction)(File* file, uint32 flags);
typedef void (*CloseFunction)(File* file);
typedef int32 (*IoctlFunction)(File *file, int32 request, void * argp);
typedef int32 (*LseekFunction)(File *file, int32 offset, int32 whence);
typedef int32 (*FtruncateFunction)(File *file, int32 length);
typedef int32 (*StatFunction)(FileSystemNode *node, struct stat *buf);
typedef FileSystemDirent * (*ReadDirFunction)(FileSystemNode*,uint32);
typedef FileSystemNode * (*FindDirFunction)(FileSystemNode*,char *name);
typedef BOOL (*MkDirFunction)(FileSystemNode* node, const char *name, uint32 flags);
typedef void* (*MmapFunction)(File* file, uint32 size, uint32 offset, uint32 flags);
typedef BOOL (*MunmapFunction)(File* file, void* address, uint32 size);

typedef BOOL (*MountFunction)(const char* sourcePath, const char* targetPath, uint32 flags, void *data);

typedef struct FileSystem
{
    char name[32];
    MountFunction checkMount;
    MountFunction mount;
} FileSystem;

typedef struct FileSystemNode
{
    char name[128];
    uint32 mask;
    uint32 userId;
    uint32 groupId;
    uint32 nodeType;
    uint32 inode;
    uint32 length;
    ReadWriteBlockFunction readBlock;
    ReadWriteBlockFunction writeBlock;
    ReadWriteFunction read;
    ReadWriteFunction write;
    ReadWriteTestFunction readTestReady;
    ReadWriteTestFunction writeTestReady;
    OpenFunction open;
    CloseFunction close;
    IoctlFunction ioctl;
    LseekFunction lseek;
    FtruncateFunction ftruncate;
    StatFunction stat;
    ReadDirFunction readdir;
    FindDirFunction finddir;
    MkDirFunction mkdir;
    MmapFunction mmap;
    MunmapFunction munmap;
    FileSystemNode *firstChild;
    FileSystemNode *nextSibling;
    FileSystemNode *parent;
    FileSystemNode *mountPoint;//only used in mounts
    FileSystemNode *mountSource;//only used in mounts
    void* privateNodeData;
} FileSystemNode;

typedef struct FileSystemDirent
{
    char name[128];
    FileType fileType;
    uint32 inode;
} FileSystemDirent;

//Per open
typedef struct File
{
    FileSystemNode* node;
    Process* process;
    Thread* thread;
    int32 fd;
    uint32 flags;
    int32 offset;
    void* private_data;
} File;

struct stat
{
    uint16/*dev_t      */ st_dev;     /* ID of device containing file */
    uint16/*ino_t      */ st_ino;     /* inode number */
    uint32/*mode_t     */ st_mode;    /* protection */
    uint16/*nlink_t    */ st_nlink;   /* number of hard links */
    uint16/*uid_t      */ st_uid;     /* user ID of owner */
    uint16/*gid_t      */ st_gid;     /* group ID of owner */
    uint16/*dev_t      */ st_rdev;    /* device ID (if special file) */
    uint32/*off_t      */ st_size;    /* total size, in bytes */

    uint32/*time_t     */ st_atime;
    uint32/*long       */ st_spare1;
    uint32/*time_t     */ st_mtime;
    uint32/*long       */ st_spare2;
    uint32/*time_t     */ st_ctime;
    uint32/*long       */ st_spare3;
    uint32/*blksize_t  */ st_blksize;
    uint32/*blkcnt_t   */ st_blocks;
    uint32/*long       */ st_spare4[2];
};


uint32 fs_read(File* file, uint32 size, uint8* buffer);
uint32 fs_write(File* file, uint32 size, uint8* buffer);
File* fs_open(FileSystemNode* node, uint32 flags);
File* fs_open_for_process(Thread* thread, FileSystemNode* node, uint32 flags);
void fs_close(File* file);
int32 fs_ioctl(File* file, int32 request, void* argp);
int32 fs_lseek(File* file, int32 offset, int32 whence);
int32 fs_ftruncate(File* file, int32 length);
int32 fs_stat(FileSystemNode *node, struct stat *buf);
FileSystemDirent* fs_readdir(FileSystemNode* node, uint32 index);
FileSystemNode* fs_finddir(FileSystemNode* node, char* name);
BOOL fs_mkdir(FileSystemNode *node, const char* name, uint32 flags);
void* fs_mmap(File* file, uint32 size, uint32 offset, uint32 flags);
BOOL fs_munmap(File* file, void* address, uint32 size);
int fs_get_node_path(FileSystemNode* node, char* buffer, uint32 buffer_size);
BOOL fs_resolve_path(const char* path, char* buffer, int buffer_size);

void fs_initialize();
FileSystemNode* fs_get_root_node();
FileSystemNode* fs_get_node(const char* path);
FileSystemNode* fs_get_node_absolute_or_relative(const char* path, Process* process);
FileSystemNode* fs_get_node_relative_to_node(const char* path, FileSystemNode* relative_to);

BOOL fs_register(FileSystem* fs);
BOOL fs_mount(const char *source, const char *target, const char *fsType, uint32 flags, void *data);
BOOL fs_check_mount(const char *source, const char *target, const char *fsType, uint32 flags, void *data);

#endif
