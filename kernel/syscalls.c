#include "syscalls.h"
#include "fs.h"
#include "process.h"
#include "screen.h"
#include "alloc.h"
#include "pipe.h"
#include "debugprint.h"
#include "desktopenvironment.h"
#include "timer.h"
#include "sleep.h"

/**************
 * All of syscall entered with interrupts disabled!
 * A syscall can enable interrupts if it is needed.
 *
 **************/

int syscall_open(const char *pathname, int flags)
{
    Process* process = getCurrentThread()->owner;
    if (process)
    {
        //Screen_PrintF("open():[%s]\n", pathname);
        FileSystemNode* node = getFileSystemNodeAbsoluteOrRelative(pathname, process);
        if (node)
        {
            //Screen_PrintF("open():node:[%s]\n", node->name);
            File* file = open_fs(node, flags);

            if (file)
            {
                return file->fd;
            }
        }
    }

    return -1;
}

int syscall_close(int fd)
{
    Process* process = getCurrentThread()->owner;
    if (process)
    {
        if (fd < MAX_OPENED_FILES)
        {
            File* file = process->fd[fd];

            if (file)
            {
                close_fs(file);

                return 0;
            }
            else
            {
                //TODO: error invalid fd
            }
        }
        else
        {
            //TODO: error invalid fd
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

int syscall_read(int fd, void *buf, int nbytes)
{
    //Screen_PrintF("syscall_read: begin - nbytes:%d\n", nbytes);

    Process* process = getCurrentThread()->owner;
    if (process)
    {
        if (fd < MAX_OPENED_FILES)
        {
            File* file = process->fd[fd];

            if (file)
            {
                //Debug_PrintF("syscall_read(%d): %s\n", process->pid, buf);

                //enableInterrupts();

                //Each handler is free to enable interrupts.
                //We don't enable them here.

                return read_fs(file, nbytes, buf);
            }
            else
            {
                //TODO: error invalid fd
            }
        }
        else
        {
            //TODO: error invalid fd
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

int syscall_write(int fd, void *buf, int nbytes)
{
    Process* process = getCurrentThread()->owner;
    if (process)
    {
        //Screen_PrintF("syscall_write() called from process: %d. fd:%d\n", process->pid, fd);

        if (fd < MAX_OPENED_FILES)
        {
            File* file = process->fd[fd];

            if (file)
            {
                /*
                for (int i = 0; i < nbytes; ++i)
                {
                    Debug_PrintF("pid:%d syscall_write:buf[%d]=%c\n", process->pid, i, ((char*)buf)[i]);
                }
                */

                uint32 writeResult = write_fs(file, nbytes, buf);

                return writeResult;
            }
            else
            {
                //TODO: error invalid fd
            }
        }
        else
        {
            //TODO: error invalid fd
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

int syscall_lseek(int fd, int offset, int whence)
{
    Process* process = getCurrentThread()->owner;
    if (process)
    {
        //Screen_PrintF("syscall_lseek() called from process: %d. fd:%d\n", process->pid, fd);

        if (fd < MAX_OPENED_FILES)
        {
            File* file = process->fd[fd];

            if (file)
            {
                return lseek_fs(file, offset, whence);
            }
            else
            {
                //TODO: error invalid fd
            }
        }
        else
        {
            //TODO: error invalid fd
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

int syscall_stat(const char *path, struct stat *buf)
{
    Process* process = getCurrentThread()->owner;
    if (process)
    {
        //Screen_PrintF("syscall_stat() called from process: %d. path:%s\n", process->pid, path);

        FileSystemNode* node = getFileSystemNodeAbsoluteOrRelative(path, process);

        if (node)
        {
            return stat_fs(node, buf);
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

int syscall_fstat(int fd, struct stat *buf)
{
    Process* process = getCurrentThread()->owner;
    if (process)
    {
        if (fd < MAX_OPENED_FILES)
        {
            File* file = process->fd[fd];

            if (file)
            {
                return stat_fs(file->node, buf);
            }
            else
            {
                //TODO: error invalid fd
            }
        }
        else
        {
            //TODO: error invalid fd
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

int syscall_ioctl(int fd, int32 request, void *arg)
{
    Process* process = getCurrentThread()->owner;
    if (process)
    {
        if (fd < MAX_OPENED_FILES)
        {
            File* file = process->fd[fd];

            if (file)
            {
                return ioctl_fs(file, request, arg);
            }
            else
            {
                //TODO: error invalid fd
            }
        }
        else
        {
            //TODO: error invalid fd
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

int syscall_exit()
{
    Process* process = getCurrentThread()->owner;
    if (process)
    {
        //Screen_PrintF("syscall_exit() for %d !!!\n", process->pid);

        destroyProcess(process);

        waitForSchedule();
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

void* syscall_sbrk(uint32 increment)
{
    //Screen_PrintF("syscall_sbrk() !!! inc:%d\n", increment);

    Process* process = getCurrentThread()->owner;
    if (process)
    {
        return sbrk(process, increment);
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return (void*)-1;
}

int syscall_fork()
{
    //Not implemented

    return -1;
}

int syscall_getpid()
{
    Process* process = getCurrentThread()->owner;
    if (process)
    {
        return process->pid;
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

//NON-posix
int syscall_execute(const char *path, char *const argv[], char *const envp[])
{
    int result = -1;

    Process* process = getCurrentThread()->owner;
    if (process)
    {
        FileSystemNode* node = getFileSystemNodeAbsoluteOrRelative(path, process);
        if (node)
        {
            File* f = open_fs(node, 0);
            if (f)
            {
                void* image = kmalloc(node->length);

                //Screen_PrintF("executing %s and its %d bytes\n", filename, node->length);

                int32 bytesRead = read_fs(f, node->length, image);

                //Screen_PrintF("syscall_execute: read_fs returned %d bytes\n", bytesRead);

                if (bytesRead > 0)
                {
                    Process* newProcess = createUserProcessFromElfData("userProcess", image, argv, envp, process, NULL);

                    if (newProcess)
                    {
                        result = newProcess->pid;
                    }
                }
                close_fs(f);

                kfree(image);
            }

        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return result;
}

int syscall_execve(const char *path, char *const argv[], char *const envp[])
{
    Process* callingProcess = getCurrentThread()->owner;

    FileSystemNode* node = getFileSystemNode(path);
    if (node)
    {
        File* f = open_fs(node, 0);
        if (f)
        {
            void* image = kmalloc(node->length);

            if (read_fs(f, node->length, image) > 0)
            {
                disableInterrupts(); //just in case if a file operation left interrupts enabled.

                Process* newProcess = createUserProcessEx("fromExecve", callingProcess->pid, 0, NULL, image, argv, envp, NULL, callingProcess->tty);

                close_fs(f);

                kfree(image);

                if (newProcess)
                {
                    destroyProcess(callingProcess);

                    waitForSchedule();

                    //unreachable
                }
            }

            close_fs(f);

            kfree(image);
        }
    }


    //if it all goes well, this line is unreachable
    return 0;
}

int syscall_wait(int *wstatus)
{
    //TODO: return pid of exited child. implement with sendsignal

    Thread* currentThread = getCurrentThread();

    Process* process = currentThread->owner;
    if (process)
    {
        Thread* thread = getMainKernelThread();
        while (thread)
        {
            if (process == thread->owner->parent)
            {
                //We have a child process

                currentThread->state = TS_WAITCHILD;

                enableInterrupts();
                while (currentThread->state == TS_WAITCHILD);

                break;
            }

            thread = thread->next;
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

int syscall_kill(int pid, int sig)
{
    Process* selfProcess = getCurrentThread()->owner;

    Thread* thread = getMainKernelThread();
    while (thread)
    {
        if (pid == thread->owner->pid)
        {
            //We have found the process

            //TODOif (sig==KILL)
            {
                destroyProcess(thread->owner);

                if (thread->owner == selfProcess)
                {
                    waitForSchedule();
                }
                else
                {
                    return 0;
                }
            }
            break;
        }

        thread = thread->next;
    }

    return -1;
}

int syscall_mount(const char *source, const char *target, const char *fsType, unsigned long flags, void *data)//non-posix
{
    BOOL result = mountFileSystem(source, target, fsType, flags, data);

    if (TRUE == result)
    {
        return 0;//on success
    }

    return -1;//on error
}

int syscall_unmount(const char *target)//non-posix
{
    FileSystemNode* targetNode = getFileSystemNode(target);

    if (targetNode)
    {
        if (targetNode->nodeType == FT_MountPoint)
        {
            targetNode->nodeType = FT_Directory;

            targetNode->mountPoint = NULL;

            //TODO: check conditions, maybe busy. make clean up.

            return 0;//on success
        }
    }

    return -1;//on error
}

int syscall_mkdir(const char *path, uint32 mode)
{
    char parentPath[128];
    const char* name = NULL;
    int length = strlen(path);
    for (int i = length - 1; i >= 0; --i)
    {
        if (path[i] == '/')
        {
            name = path + i + 1;
            strncpy(parentPath, path, i);
            parentPath[i] = '\0';
            break;
        }
    }

    if (strlen(parentPath) == 0)
    {
        parentPath[0] = '/';
        parentPath[1] = '\0';
    }

    if (strlen(name) == 0)
    {
        return -1;
    }

    //Screen_PrintF("mkdir: parent:[%s] name:[%s]\n", parentPath, name);

    FileSystemNode* targetNode = getFileSystemNode(parentPath);

    if (targetNode)
    {
        BOOL success = mkdir_fs(targetNode, name, mode);
        if (success)
        {
            return 0;//on success
        }
    }

    return -1;//on error
}

int syscall_rmdir(const char *path)
{
    //TODO:
    //return 0;//on success

    return -1;//on error
}

int syscall_getdents(int fd, char *buf, int nbytes)
{
    Process* process = getCurrentThread()->owner;
    if (process)
    {
        if (fd < MAX_OPENED_FILES)
        {
            File* file = process->fd[fd];

            if (file)
            {
                //Screen_PrintF("syscall_getdents(%d): %s\n", process->pid, buf);

                int byteCounter = 0;

                int index = 0;
                FileSystemDirent* dirent = readdir_fs(file->node, index);

                while (NULL != dirent && (byteCounter + sizeof(FileSystemDirent) <= nbytes))
                {
                    memcpy((uint8*)buf + byteCounter, (uint8*)dirent, sizeof(FileSystemDirent));

                    byteCounter += sizeof(FileSystemDirent);

                    index += 1;
                    dirent = readdir_fs(file->node, index);
                }

                return byteCounter;
            }
            else
            {
                //TODO: error invalid fd
            }
        }
        else
        {
            //TODO: error invalid fd
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;//on error
}

int syscall_readDir(int fd, void *dirent, int index)
{
    Process* process = getCurrentThread()->owner;
    if (process)
    {
        if (fd < MAX_OPENED_FILES)
        {
            File* file = process->fd[fd];

            if (file)
            {
                FileSystemDirent* direntFs = readdir_fs(file->node, index);

                if (direntFs)
                {
                    memcpy((uint8*)dirent, (uint8*)direntFs, sizeof(FileSystemDirent));

                    return 1;
                }
            }
            else
            {
                //TODO: error invalid fd
            }
        }
        else
        {
            //TODO: error invalid fd
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;//on error
}

int syscall_getWorkingDirectory(char *buf, int size)
{
    Process* process = getCurrentThread()->owner;
    if (process)
    {
        if (process->workingDirectory)
        {
            return getFileSystemNodePath(process->workingDirectory, buf, size);
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;//on error
}

int syscall_setWorkingDirectory(const char *path)
{
    Process* process = getCurrentThread()->owner;
    if (process)
    {
        FileSystemNode* node = getFileSystemNodeAbsoluteOrRelative(path, process);

        if (node)
        {
            process->workingDirectory = node;

            return 0; //success
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;//on error
}

int syscall_managePipe(const char *pipeName, int operation, int data)
{
    int result = -1;

    switch (operation)
    {
    case 0:
        result = existsPipe(pipeName);
        break;
    case 1:
        result = createPipe(pipeName, data);
        break;
    case 2:
        result = destroyPipe(pipeName);
        break;
    }

    return result;
}

int syscall_manageWindow(int command, int parameter1, int parameter2, int parameter3)
{
    Thread* thread = getCurrentThread();

    switch (command)
    {
    case 0:
        DE_DestroyWindow((Window*)parameter1);
        break;
    case 1:
        return (int)DE_CreateWindow(DE_GetDefault(), parameter1, parameter2, thread);
        break;
    case 2:
        DE_SetWindowPosition((Window*)parameter1, parameter2, parameter3);
        break;
    case 3:
        DE_CopyToWindowBuffer((Window*)parameter1, (const uint8*)parameter2);
        break;
    default:
        break;
    }

    return 0;
}

int syscall_getUptimeMilliseconds()
{
    return getUptimeMilliseconds();
}

int syscall_sleepMilliseconds(int ms)
{
    Thread* thread = getCurrentThread();

    sleepMilliseconds(thread, (uint32)ms);

    return 0;
}
