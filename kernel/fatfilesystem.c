#include "fatfilesystem.h"
#include "common.h"
#include "fs.h"
#include "alloc.h"
#include "fatfs_ff.h"
#include "fatfs_diskio.h"

#define SEEK_SET	0	/* Seek from beginning of file.  */
#define SEEK_CUR	1	/* Seek from current position.  */
#define SEEK_END	2

static BOOL mount(const char* source_path, const char* target_path, uint32_t flags, void *data);
static BOOL checkMount(const char* sourcePath, const char* targetPath, uint32_t flags, void *data);
static FileSystemDirent* readdir(FileSystemNode *node, uint32_t index);
static FileSystemNode* finddir(FileSystemNode *node, char *name);
static int32_t read(File *file, uint32_t size, uint8_t *buffer);
static int32_t write(File *file, uint32_t size, uint8_t *buffer);
static int32_t lseek(File *file, int32_t offset, int32_t whence);
static int32_t stat(FileSystemNode *node, struct stat* buf);
static BOOL open(File *file, uint32_t flags);
static void close(File *file);

static FileSystemDirent g_fs_dirent;

static FileSystemNode* g_mounted_block_devices[FF_VOLUMES];


void fatfs_initialize()
{
    FileSystem fs;
    memset((uint8_t*)&fs, 0, sizeof(fs));
    strcpy(fs.name, "fat");
    fs.mount = mount;
    fs.check_mount = checkMount;

    fs_register(&fs);

    for (int i = 0; i < FF_VOLUMES; ++i)
    {
        g_mounted_block_devices[i] = NULL;
    }
}

static BOOL mount(const char* source_path, const char* target_path, uint32_t flags, void *data)
{
    printkf("fat mount source: %s\n", source_path);

    FileSystemNode* node = fs_get_node(source_path);
    if (node && node->node_type == FT_BLOCK_DEVICE)
    {
        FileSystemNode* target_node = fs_get_node(target_path);
        if (target_node)
        {
            if (target_node->node_type == FT_DIRECTORY)
            {
                printkf("fat mount target: %s\n", target_path);

                int32_t volume = -1;
                for (int32_t v = 0; v < FF_VOLUMES; ++v)
                {
                    if (NULL == g_mounted_block_devices[v])
                    {
                        volume = v;
                        break;
                    }
                }

                if (volume < 0)
                {
                    return FALSE;
                }

                FileSystemNode* new_node = kmalloc(sizeof(FileSystemNode));

                memset((uint8_t*)new_node, 0, sizeof(FileSystemNode));
                strcpy(new_node->name, target_node->name);
                new_node->node_type = FT_DIRECTORY;
                new_node->open = open;
                new_node->readdir = readdir;
                new_node->finddir = finddir;
                new_node->parent = target_node->parent;
                new_node->mount_source = node;
                new_node->private_node_data = (void*)volume;

                g_mounted_block_devices[volume] = node;

                FATFS* fatFs = (FATFS*)kmalloc(sizeof(FATFS));
                //uint8_t work[512];
                //FRESULT fr = f_mkfs("", FM_FAT | FM_SFD, 512, work, 512);
                //Screen_PrintF("f_mkfs: %d\n", fr);
                char path[8];
                sprintf(path, 8, "%d:", volume);
                FRESULT fr = f_mount(fatFs, path, 1);
                //Screen_PrintF("f_mount: fr:%d drv:%d\n", fr, fatFs->pdrv);

                if (FR_OK == fr)
                {
                    target_node->node_type |= FT_MOUNT_POINT;
                    target_node->mount_point = new_node;

                    return TRUE;
                }
                else
                {
                    kfree(new_node);

                    kfree(fatFs);

                    g_mounted_block_devices[volume] = NULL;
                }
            }
        }
    }

    return FALSE;
}

static BOOL checkMount(const char* source_path, const char* target_path, uint32_t flags, void *data)
{
    FileSystemNode* node = fs_get_node(source_path);
    if (node && node->node_type == FT_BLOCK_DEVICE)
    {
        FileSystemNode* targetNode = fs_get_node(target_path);
        if (targetNode)
        {
            if (targetNode->node_type == FT_DIRECTORY)
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}

static FileSystemDirent* readdir(FileSystemNode *node, uint32_t index)
{
    //when node is the root of mounted filesystem,
    //node->mount_source is the source node (eg. disk partition /dev/hd1p1)

    //Screen_PrintF("readdir1: node->name:%s\n", node->name);

    uint8_t target_path[128];

    FileSystemNode *n = node;
    int char_index = 126;
    memset(target_path, 0, 128);
    while (NULL == n->mount_source)
    {
        int length = strlen(n->name);

        char_index -= length;

        if (char_index < 2)
        {
            return NULL;
        }

        strcpy_nonnull((char*)(target_path + char_index), n->name);
        char_index -= 1;
        target_path[char_index] = '/';

        n = n->parent;
    }

    char number[8];
    sprintf(number, 8, "%d", n->private_node_data);//volume nuber

    target_path[char_index] = ':';
    int length = strlen(number);
    char_index -= length;
    if (char_index < 0)
    {
        return NULL;
    }

    strcpy_nonnull((char*)(target_path + char_index), number);
    uint8_t* target = target_path + char_index;

    //Screen_PrintF("readdir: targetpath:[%s]\n", target);

    DIR dir;
    FRESULT fr = f_opendir(&dir, (TCHAR*)target);
    if (FR_OK == fr)
    {
        FILINFO fileInfo;
        for (int i = 0; i <= index; ++i)
        {
            memset((uint8_t*)&fileInfo, 0, sizeof(FILINFO));
            fr = f_readdir(&dir, &fileInfo);

            if (strlen(fileInfo.fname) <= 0)
            {
                f_closedir(&dir);

                return NULL;
            }
        }

        g_fs_dirent.inode = 0;
        strcpy(g_fs_dirent.name, fileInfo.fname);
        if ((fileInfo.fattrib & AM_DIR) == AM_DIR)
        {
            g_fs_dirent.file_type = FT_DIRECTORY;
        }
        else
        {
            g_fs_dirent.file_type = FT_FILE;
        }

        f_closedir(&dir);

        return &g_fs_dirent;
    }

    return NULL;
}

static FileSystemNode* finddir(FileSystemNode *node, char *name)
{
    //when node is the root of mounted filesystem,
    //node->mount_source is the source node (eg. disk partition /dev/hd1p1)

    //Screen_PrintF("finddir1: node->name:%s name:%s\n", node->name, name);

    FileSystemNode* child = node->first_child;
    while (NULL != child)
    {
        if (strcmp(name, child->name) == 0)
        {
            return child;
        }

        child = child->next_sibling;
    }

    //If we are here, this file is accesed first time in this session.
    //So we create its node...

    uint8_t target_path[128];

    FileSystemNode *n = node;
    int char_index = 126;
    memset(target_path, 0, 128);
    int length = strlen(name);
    char_index -= length;
    strcpy_nonnull((char*)(target_path + char_index), name);
    char_index -= 1;
    target_path[char_index] = '/';
    while (NULL == n->mount_source)
    {
        length = strlen(n->name);
        char_index -= length;

        if (char_index < 2)
        {
            return NULL;
        }

        strcpy_nonnull((char*)(target_path + char_index), n->name);
        char_index -= 1;
        target_path[char_index] = '/';

        n = n->parent;
    }

    char number[8];
    sprintf(number, 8, "%d", n->private_node_data);//volume nuber

    target_path[char_index] = ':';
    length = strlen(number);
    char_index -= length;
    if (char_index < 0)
    {
        return NULL;
    }

    strcpy_nonnull((char*)(target_path + char_index), number);
    uint8_t* target = target_path + char_index;

    //Screen_PrintF("finddir: targetpath:[%s]\n", target);

    FILINFO file_info;
    memset((uint8_t*)&file_info, 0, sizeof(FILINFO));
    FRESULT fr = f_stat((TCHAR*)target, &file_info);
    if (FR_OK == fr)
    {
        FileSystemNode* new_node = kmalloc(sizeof(FileSystemNode));

        memset((uint8_t*)new_node, 0, sizeof(FileSystemNode));
        strcpy(new_node->name, name);
        new_node->parent = node;
        new_node->readdir = readdir;
        new_node->finddir = finddir;
        new_node->open = open;
        new_node->close = close;
        new_node->read = read;
        new_node->write = write;
        new_node->lseek = lseek;
        new_node->stat = stat;
        new_node->length = file_info.fsize;

        if ((file_info.fattrib & AM_DIR) == AM_DIR)
        {
            new_node->node_type = FT_DIRECTORY;
        }
        else
        {
            new_node->node_type = FT_FILE;
        }

        if (NULL == node->first_child)
        {
            node->first_child = new_node;
        }
        else
        {
            FileSystemNode* child = node->first_child;
            while (NULL != child->next_sibling)
            {
                child = child->next_sibling;
            }
            child->next_sibling = new_node;
        }

        //Screen_PrintF("finddir: returning [%s]\n", name);
        return new_node;
    }
    else
    {
        //Screen_PrintF("finddir error: fr: %d]\n", fr);
    }

    return NULL;
}

static int32_t read(File *file, uint32_t size, uint8_t *buffer)
{
    if (file->private_data == NULL)
    {
        return -1;
    }

    FIL* f = (FIL*)file->private_data;

    UINT br = 0;
    FRESULT fr = f_read(f, buffer, size, &br);
    file->offset = f->fptr;
    //Screen_PrintF("fat read: name:%s size:%d hasRead:%d, fr:%d\n", file->node->name, size, br, fr);
    if (FR_OK == fr)
    {
        return br;
    }

    return -1;
}

static int32_t write(File *file, uint32_t size, uint8_t *buffer)
{
    if (file->private_data == NULL)
    {
        return -1;
    }

    FIL* f = (FIL*)file->private_data;

    UINT bw = 0;
    FRESULT fr = f_write(f, buffer, size, &bw);
    file->offset = f->fptr;
    if (FR_OK == fr)
    {
        return bw;
    }

    return -1;
}

static int32_t lseek(File *file, int32_t offset, int32_t whence)
{
    if (file->private_data == NULL)
    {
        return -1;
    }

    FIL* f = (FIL*)file->private_data;

    FRESULT fr = FR_INVALID_OBJECT;

    switch (whence)
    {
    case SEEK_SET:
        fr = f_lseek(f, offset);
        break;
    case SEEK_CUR:
        fr = f_lseek(f, f_tell(f) + offset);
        break;
    case SEEK_END:
        fr = f_lseek(f, f_size(f) + offset);
        break;
    default:
        break;
    }


    if (FR_OK == fr)
    {
        file->offset = f->fptr;

        return file->offset;
    }

    return -1;
}

static int32_t stat(FileSystemNode *node, struct stat* buf)
{
    //Screen_PrintF("fat stat [%s]\n", node->name);

    uint8_t target_path[128];

    FileSystemNode *n = node;
    int char_index = 126;
    memset(target_path, 0, 128);
    while (NULL == n->mount_source)
    {
        int length = strlen(n->name);
        char_index -= length;

        if (char_index < 2)
        {
            return NULL;
        }

        strcpy_nonnull((char*)(target_path + char_index), n->name);
        char_index -= 1;
        target_path[char_index] = '/';

        n = n->parent;
    }

    char number[8];
    sprintf(number, 8, "%d", n->private_node_data);//volume nuber

    target_path[char_index] = ':';
    int length = strlen(number);
    char_index -= length;
    if (char_index < 0)
    {
        return NULL;
    }

    strcpy_nonnull((char*)(target_path + char_index), number);
    uint8_t* target = target_path + char_index;

    //Screen_PrintF("fat stat target:[%s]\n", target);

    FILINFO file_info;
    memset((uint8_t*)&file_info, 0, sizeof(FILINFO));
    FRESULT fr = f_stat((TCHAR*)target, &file_info);
    if (FR_OK == fr)
    {
        if ((file_info.fattrib & AM_DIR) == AM_DIR)
        {
            node->node_type = FT_DIRECTORY;
        }
        else
        {
            node->node_type = FT_FILE;
        }

        node->length = file_info.fsize;

        return 1;
    }

    return -1; //Error
}

static BOOL open(File *file, uint32_t flags)
{
    //Screen_PrintF("fat open %s\n", file->node->name);

    FileSystemNode *node = file->node;

    if (node->node_type == FT_DIRECTORY)
    {
        return TRUE;
    }

    uint8_t target_path[128];

    FileSystemNode *n = node;
    int char_index = 126;
    memset(target_path, 0, 128);
    while (NULL == n->mount_source)
    {
        int length = strlen(n->name);
        char_index -= length;

        if (char_index < 2)
        {
            return NULL;
        }

        strcpy_nonnull((char*)(target_path + char_index), n->name);
        char_index -= 1;
        target_path[char_index] = '/';

        n = n->parent;
    }

    char number[8];
    sprintf(number, 8, "%d", n->private_node_data);//volume nuber

    target_path[char_index] = ':';
    int length = strlen(number);
    char_index -= length;
    if (char_index < 0)
    {
        return NULL;
    }

    strcpy_nonnull((char*)(target_path + char_index), number);
    uint8_t* target = target_path + char_index;

    //Screen_PrintF("fat open %s\n", target);

    int fatfs_mode = FA_READ;

    switch (flags)
    {
    case O_RDONLY:
        fatfs_mode = FA_READ;
        break;
    case O_WRONLY:
        fatfs_mode = FA_WRITE;
        break;
    case O_RDWR:
        fatfs_mode = (FA_READ | FA_WRITE);
        break;
        //TODO: append, create
    default:
        break;
    }

    FIL* f = (FIL*)kmalloc(sizeof(FIL));
    FRESULT fr = f_open(f, (TCHAR*)target, fatfs_mode);
    if (FR_OK == fr)
    {
        file->offset = f->fptr;

        file->private_data = f;

        return TRUE;
    }

    return FALSE;
}

static void close(File *file)
{
    if (file->private_data == NULL)
    {
        return;
    }

    FIL* f = (FIL*)file->private_data;

    f_close(f);

    kfree(f);

    file->private_data = NULL;
}

DSTATUS disk_initialize(
        BYTE pdrv		//Physical drive nmuber
)
{
    return 0;
}

DSTATUS disk_status(BYTE pdrv)
{
    return 0;
}

DRESULT disk_read (
    BYTE pdrv,			/* Physical drive nmuber (0) */
    BYTE *buff,			/* Pointer to the data buffer to store read data */
    DWORD sector,		/* Start sector number (LBA) */
    UINT count			/* Number of sectors to read */
)
{
    //Screen_PrintF("disk_read() drv:%d sector:%d count:%d\n", pdrv, sector, count);

    if (g_mounted_block_devices[pdrv] == NULL) return RES_NOTRDY;

    //if (sector >= RamDiskSize) return RES_PARERR;

    g_mounted_block_devices[pdrv]->read_block(g_mounted_block_devices[pdrv], (uint32_t)sector, count, buff);

    return RES_OK;
}

DRESULT disk_write (
    BYTE pdrv,			/* Physical drive nmuber (0) */
    const BYTE *buff,	/* Pointer to the data to be written */
    DWORD sector,		/* Start sector number (LBA) */
    UINT count			/* Number of sectors to write */
)
{
    if (g_mounted_block_devices[pdrv] == NULL) return RES_NOTRDY;

    //if (sector >= RamDiskSize) return RES_PARERR;

    g_mounted_block_devices[pdrv]->write_block(g_mounted_block_devices[pdrv], (uint32_t)sector, count, (uint8_t*)buff);

    return RES_OK;
}

DRESULT disk_ioctl (
    BYTE pdrv,		/* Physical drive nmuber (0) */
    BYTE ctrl,		/* Control code */
    void* buff		/* Buffer to send/receive data block */
)
{
    if (g_mounted_block_devices[pdrv] == NULL) return RES_ERROR;

    DRESULT dr = RES_ERROR;

    File* f = NULL;

    uint32_t value = 0;

    switch (ctrl)
    {
    case CTRL_SYNC:
        dr = RES_OK;
        break;
    case GET_SECTOR_COUNT:
        f = fs_open(g_mounted_block_devices[pdrv], 0);
        if (f)
        {
            fs_ioctl(f, IC_GET_SECTOR_COUNT, &value);
            *(DWORD*)buff = value;
            dr = RES_OK;
            fs_close(f);
        }
        printkf("disk_ioctl GET_SECTOR_COUNT: %d\n", value);
        break;
    case GET_BLOCK_SIZE:
        f = fs_open(g_mounted_block_devices[pdrv], 0);
        if (f)
        {
            fs_ioctl(f, IC_GET_SECTOR_SIZE_BYTES, &value);
            *(DWORD*)buff = value;
            dr = RES_OK;
            fs_close(f);
        }
        printkf("disk_ioctl GET_BLOCK_SIZE: %d\n", value);
        *(DWORD*)buff = value;
        dr = RES_OK;
        break;
    }
    return dr;
}
