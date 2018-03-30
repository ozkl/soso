#include "screen.h"
#include "descriptortables.h"
#include "timer.h"
#include "multiboot.h"
#include "fs.h"
#include "syscall.h"
#include "syscalls.h"
#include "serial.h"
#include "isr.h"
#include "vmm.h"
#include "alloc.h"
#include "process.h"
#include "keyboard.h"
#include "ttydriver.h"
#include "devfs.h"
#include "systemfs.h"
#include "pipe.h"
#include "random.h"
#include "elf.h"
#include "debugprint.h"
#include "ramdisk.h"
#include "fatfilesystem.h"
#include "vbe.h"
#include "fifobuffer.h"
#include "gfx.h"

extern uint32 _start;
extern uint32 _end;
uint32 gPhysicalKernelStartAddress = (uint32)&_start;
uint32 gPhysicalKernelEndAddress = (uint32)&_end;

static void* locateInitrd(struct Multiboot *mbi, uint32* size)
{
    if (mbi->mods_count > 0)
    {
        uint32 startLocation = *((uint32*)mbi->mods_addr);
        uint32 endLocation = *(uint32*)(mbi->mods_addr + 4);

        *size = endLocation - startLocation;

        return (void*)startLocation;
    }

    return NULL;
}

static void listFs2(const char* path)
{
    //Screen_PrintF("listFs2 - 1:[%s]\n", path);

    int fd = trigger_syscall_open(path, 0);
    if (fd < 0)
    {
        return;
    }

    //Screen_PrintF("listFs2 - 2\n");

    char buffer[1024];
    int bytesRead = trigger_syscall_getdents(fd, buffer, 1024);

    if (bytesRead < 0)
    {
        trigger_syscall_close(fd);
        return;
    }

    //Screen_PrintF("listFs2 - 3\n");


    int byteCounter = 0;

    while (byteCounter + sizeof(FileSystemDirent) <= bytesRead)
    {
        FileSystemDirent *dirEntry = (FileSystemDirent*)(buffer + byteCounter);

        if ((dirEntry->fileType & FT_MountPoint) == FT_MountPoint)
        {
            Screen_PrintF("m");
        }
        else if (dirEntry->fileType == FT_File)
        {
            Screen_PrintF("f");
        }
        else if (dirEntry->fileType == FT_Directory)
        {
            Screen_PrintF("d");
        }
        else if (dirEntry->fileType == FT_BlockDevice)
        {
            Screen_PrintF("b");
        }
        else if (dirEntry->fileType == FT_CharacterDevice)
        {
            Screen_PrintF("c");
        }
        else if (dirEntry->fileType == FT_Pipe)
        {
            Screen_PrintF("p");
        }
        else if (dirEntry->fileType == FT_SymbolicLink)
        {
            Screen_PrintF("l");
        }
        else
        {
            Screen_PrintF("*%d", dirEntry->fileType);
        }

        Screen_PrintF("   %s\n", dirEntry->name);

        byteCounter += sizeof(FileSystemDirent);
    }

    trigger_syscall_close(fd);
}

void printUsageInfo()
{
    char buffer[164];

    while (TRUE)
    {
        sprintf(buffer, "Used kheap:%d", getKernelHeapUsed());
        Screen_Print(0, 60, buffer);

        sprintf(buffer, "Pages:%d/%d       ", getUsedPageCount(), getTotalPageCount());
        Screen_Print(1, 60, buffer);

        sprintf(buffer, "Uptime:%d sec   ", getUptimeSeconds());
        Screen_Print(2, 60, buffer);

        Thread* p = getMainKernelThread();

        int line = 3;
        sprintf(buffer, "[idle]   cs:%d   ", p->totalContextSwitchCount);
        Screen_Print(line++, 60, buffer);
        p = p->next;
        while (p != NULL)
        {
            sprintf(buffer, "thread:%d cs:%d   ", p->threadId, p->totalContextSwitchCount);

            Screen_Print(line++, 60, buffer);

            p = p->next;
        }
    }
}

void userProcessTestFork()
{
    char bufferIn[300];
    char bufferOut[300];

    sprintf(bufferOut, "userProcessTestFork() started (%d)\n", trigger_syscall_getpid());
    trigger_syscall_write(1, bufferOut, strlen(bufferOut));

    int pid = trigger_syscall_fork();

    sprintf(bufferOut, "forked i read pid as %d\n", pid);
    trigger_syscall_write(1, bufferOut, strlen(bufferOut));

    if (0 == pid)
    {
        sprintf(bufferOut, "i am the child :)\n");
        trigger_syscall_write(1, bufferOut, strlen(bufferOut));
    }
    else
    {
        sprintf(bufferOut, "i am the parent and my child is %d :)\n", pid);
        trigger_syscall_write(1, bufferOut, strlen(bufferOut));
    }

    sprintf(bufferOut, "wha!!\n");
    trigger_syscall_write(1, bufferOut, strlen(bufferOut));

    trigger_syscall_exit();
}

void userProcessTestExecve()
{
    char bufferIn[300];
    char bufferOut[300];

    sprintf(bufferOut, "userProcessTestExecve() started (%d)\n", trigger_syscall_getpid());
    trigger_syscall_write(1, bufferOut, strlen(bufferOut));

    int result = trigger_syscall_execve("/random", NULL, NULL);

    sprintf(bufferOut, "userProcessTestExecve() reached here?\n");
    trigger_syscall_write(1, bufferOut, strlen(bufferOut));

    trigger_syscall_exit();
}

void userProcessShell()
{
    char bufferIn[300];
    char bufferOut[300];

    sprintf(bufferOut, "Kernel shell v0.1 (pid:%d)\n", syscall_getpid());
    trigger_syscall_write(1, bufferOut, strlen(bufferOut));


    while (1)
    {
        trigger_syscall_write(1, ">", 1);
        memset((uint8*)bufferIn, 0, 300);
        int nread = trigger_syscall_read(0, bufferIn, 100);
        if (nread > 0)
        {
            if (bufferIn[nread - 1] == '\n')
            {
                bufferIn[nread - 1] = '\0';
            }
            //trigger_syscall_write(1, buffer, nread);
            if (bufferIn[0] == '/')
            {
                //trying to execute something

                char* argv[] = {"shell", NULL};
                char* envp[] = {"HOME=/", "PATH=/initrd:/bin", NULL};

                int result = trigger_syscall_execute(bufferIn, argv, envp);

                if (result >= 0)
                {
                    sprintf(bufferOut, "Started pid:%d\n", result);
                    trigger_syscall_write(1, bufferOut, strlen(bufferOut));

                    int waitStatus = 0;
                    trigger_syscall_wait(&waitStatus);

                    sprintf(bufferOut, "Exited pid:%d\n", result);
                    trigger_syscall_write(1, bufferOut, strlen(bufferOut));
                }
                if (result == -1)
                {
                    sprintf(bufferOut, "Could not execute:%s\n", bufferIn);
                    trigger_syscall_write(1, bufferOut, strlen(bufferOut));
                }
            }
            else
            {
                if (strncmp(bufferIn, "kill", 4) == 0)
                {
                    int number = atoi(bufferIn + 5);

                    sprintf(bufferOut, "Killing:%d\n", number);
                    trigger_syscall_write(1, bufferOut, strlen(bufferOut));

                    trigger_syscall_kill(number, 0);
                }
                else if (strncmp(bufferIn, "fork", 4) == 0)
                {
                    int number = trigger_syscall_fork();

                    if (number == 0)
                    {
                        sprintf(bufferOut, "fork I am zero\n");
                        trigger_syscall_write(1, bufferOut, strlen(bufferOut));
                    }
                    else
                    {
                        sprintf(bufferOut, "fork I am %d\n", number);
                        trigger_syscall_write(1, bufferOut, strlen(bufferOut));
                    }
                }
                else if (strncmp(bufferIn, "ls", 2) == 0)
                {
                    const char* path = bufferIn + 3;
                    listFs2(path);
                }
                else if (strncmp(bufferIn, "mkdir", 5) == 0)
                {
                    const char* path = bufferIn + 6;

                    trigger_syscall_mkdir(path, 0);
                }
                else if (strncmp(bufferIn, "mount", 5) == 0)
                {
                    const char* path = bufferIn + 6;

                    int spaceIndex = strFirstIndexOf(path, ' ');
                    if (spaceIndex >= 0)
                    {
                        char source[64];
                        strncpy(source, path, spaceIndex);
                        source[spaceIndex] = '\0';

                        const char* target = path + spaceIndex + 1;

                        if (strlen(target) > 0)
                        {
                            sprintf(bufferOut, "Mounting [%s] to [%s] as [%s]\n", source, target, "fat");
                            trigger_syscall_write(1, bufferOut, strlen(bufferOut));

                            trigger_syscall_mount(source, target, "fat", 0, NULL);
                        }
                    }
                }
                else if (strncmp(bufferIn, "env", 3) == 0)
                {
                    char** const argvenv = (char**)USER_ARGV_ENV_LOC;

                    int i = 0;
                    const char* a = argvenv[0];
                    //args
                    while (NULL != a)
                    {
                        //sprintf(bufferOut, "%s\n", a);
                        //trigger_syscall_write(1, bufferOut, strlen(bufferOut));

                        a = argvenv[++i];
                    }

                    ++i;

                    a = argvenv[i];
                    //env
                    while (NULL != a)
                    {
                        sprintf(bufferOut, "%s\n", a);
                        trigger_syscall_write(1, bufferOut, strlen(bufferOut));

                        a = argvenv[++i];
                    }
                }
                else if (strncmp(bufferIn, "ps", 2) == 0)
                {

                }
            }
        }
        //halt();
    }

    sprintf(bufferOut, "Shell quiting\n");
    trigger_syscall_write(1, bufferOut, strlen(bufferOut));

    trigger_syscall_exit();
}

int executeFile(const char *path, char *const argv[], char *const envp[], FileSystemNode* tty)
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

                int32 bytesRead = read_fs(f, node->length, image);

                if (bytesRead > 0)
                {
                    Process* newProcess = createUserProcessFromElfData("userProcess", image, argv, envp, process, tty);

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

    return result;
}

int kmain(struct Multiboot *mboot_ptr)
{
    int stack = 5;

    initializeDescriptorTables();

    Screen_Clear();

    initializeSerial();

    Screen_PrintF("\n");
    Screen_PrintF("Lower Memory: %d KB\n", mboot_ptr->mem_lower);
    Screen_PrintF("Upper Memory: %d KB\n", mboot_ptr->mem_upper);

    uint32 memoryKb = 96*1024;
    initializeMemory(memoryKb);

    Screen_PrintF("Memory initialized for %d MB\n", memoryKb / 1024);

    Screen_PrintF("Kernel start: %x - end:%x\n", gPhysicalKernelStartAddress, gPhysicalKernelEndAddress);

    Screen_PrintF("Initial stack: %x\n", &stack);

    Screen_PrintF("Kernel cmdline:");
    if (0 != mboot_ptr->cmdline)
    {
        Screen_PrintF("%s", (char*)mboot_ptr->cmdline);
    }
    Screen_PrintF("\n");

    initializeVFS();
    initializeDevFS();
    initializeSystemFS();
    initializePipes();

    initializeTasking();

    initialiseSyscalls();
    Screen_PrintF("System calls initialized!\n");

    initializeTimer();

    initializeKeyboard();

    initializeTTYs();

    Debug_initialize("/dev/tty10");

    Serial_PrintF("pitch:%d\n", mboot_ptr->framebuffer_pitch);

    //Gfx_Initialize((uint32*)(uint32)mboot_ptr->framebuffer_addr, mboot_ptr->framebuffer_width, mboot_ptr->framebuffer_height, mboot_ptr->framebuffer_bpp / 32, mboot_ptr->framebuffer_pitch);
    //Gfx_PutCharAt('a', 10, 10, 255 << 16, 0xFFFFFFFF);

    initializeRandom();

    createRamdisk("ramdisk1", 12*1024*1024);

    initializeFatFileSystem();

    Screen_PrintF("System started!\n");

    char* argv[] = {"shell", NULL};
    char* envp[] = {"HOME=/", "PATH=/initrd", NULL};

    uint32 initrdSize = 0;
    uint8* initrdLocation = locateInitrd(mboot_ptr, &initrdSize);
    if (initrdLocation == NULL)
    {
        Screen_PrintF("Initrd not found!\n");

        Screen_PrintF("Starting kernel-shell (userspace) on TTY1 and TTY2\n");

        createUserProcessFromFunction("shell", userProcessShell, argv, envp, NULL, getFileSystemNode("/dev/tty1"));
        createUserProcessFromFunction("shell", userProcessShell, argv, envp, NULL, getFileSystemNode("/dev/tty2"));
    }
    else
    {
        Screen_PrintF("Initrd found at %x (%d bytes)\n", initrdLocation, initrdSize);
        memcpy((uint8*)*(uint32*)getFileSystemNode("/dev/ramdisk1")->privateNodeData, initrdLocation, initrdSize);
        BOOL mountSuccess = mountFileSystem("/dev/ramdisk1", "/initrd", "fat", 0, 0);

        if (mountSuccess)
        {
            Screen_PrintF("Starting shell on TTYs\n");

            executeFile("/initrd/shell", argv, envp, getFileSystemNode("/dev/tty1"));
            executeFile("/initrd/shell", argv, envp, getFileSystemNode("/dev/tty2"));
            executeFile("/initrd/shell", argv, envp, getFileSystemNode("/dev/tty3"));
            executeFile("/initrd/shell", argv, envp, getFileSystemNode("/dev/tty4"));
        }
        else
        {
            Screen_PrintF("Mounting initrd failed!\n");
        }
    }

    //createKernelThread(kernelThread2);


    //createUserProcessFromFunction("testfork", userProcessTestFork, getFileSystemNode("/dev/tty1"));
    //createUserProcessFromFunction("testExecve", userProcessTestExecve, getFileSystemNode("/dev/tty1"));


    //createKernelThread(kernelThread3);

    trigger_syscall_managePipe("pipe1", 1, 8);


    enableScheduler();

    enableInterrupts();


    uint32* data = kmalloc(120);

    while(1)
    {
        //data[0] = 555;
        //Screen_PrintF("kernelProcess1() %x\n", data);

        //data = kmalloc(120);
        //kfree(data);

        printUsageInfo();

        halt();
    }

    return 0;
}
