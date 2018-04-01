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
#include "desktopenvironment.h"

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

void printUsageInfo()
{
    char buffer[164];

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

    initializeSerial();

    uint32 memoryKb = 96*1024;
    initializeMemory(memoryKb);

    initializeVFS();
    initializeDevFS();

    Gfx_Initialize((uint32*)(uint32)mboot_ptr->framebuffer_addr, mboot_ptr->framebuffer_width, mboot_ptr->framebuffer_height, mboot_ptr->framebuffer_bpp / 32, mboot_ptr->framebuffer_pitch);
    DesktopEnvironment* desktopEnvironment = DE_Create(mboot_ptr->framebuffer_width, mboot_ptr->framebuffer_height);
    DE_SetDefault(desktopEnvironment);

    initializeTTYs(TRUE);
    //printkf works after TTY initialization

    printkf("Lower Memory: %d KB\n", mboot_ptr->mem_lower);
    printkf("Upper Memory: %d KB\n", mboot_ptr->mem_upper);
    printkf("Memory initialized for %d MB\n", memoryKb / 1024);
    printkf("Kernel start: %x - end:%x\n", gPhysicalKernelStartAddress, gPhysicalKernelEndAddress);
    printkf("Initial stack: %x\n", &stack);

    initializeSystemFS();
    initializePipes();

    initializeTasking();

    initialiseSyscalls();

    initializeTimer();

    initializeKeyboard();

    if (0 != mboot_ptr->cmdline)
    {
        printkf("Kernel cmdline:%s\n", (char*)mboot_ptr->cmdline);
    }

    Debug_initialize("/dev/tty9");

    Serial_PrintF("pitch:%d\n", mboot_ptr->framebuffer_pitch);

    initializeRandom();

    createRamdisk("ramdisk1", 12*1024*1024);

    initializeFatFileSystem();

    printkf("System started!\n");

    Window* win = DE_CreateWindow(desktopEnvironment, 300, 200, getMainKernelThread());

    char* argv[] = {"shell", NULL};
    char* envp[] = {"HOME=/", "PATH=/initrd", NULL};

    uint32 initrdSize = 0;
    uint8* initrdLocation = locateInitrd(mboot_ptr, &initrdSize);
    if (initrdLocation == NULL)
    {
        PANIC("Initrd not found!\n");
    }
    else
    {
        printkf("Initrd found at %x (%d bytes)\n", initrdLocation, initrdSize);
        memcpy((uint8*)*(uint32*)getFileSystemNode("/dev/ramdisk1")->privateNodeData, initrdLocation, initrdSize);
        BOOL mountSuccess = mountFileSystem("/dev/ramdisk1", "/initrd", "fat", 0, 0);

        if (mountSuccess)
        {
            printkf("Starting shell on TTYs\n");

            executeFile("/initrd/shell", argv, envp, getFileSystemNode("/dev/tty1"));
            executeFile("/initrd/shell", argv, envp, getFileSystemNode("/dev/tty2"));
            executeFile("/initrd/shell", argv, envp, getFileSystemNode("/dev/tty3"));
            executeFile("/initrd/shell", argv, envp, getFileSystemNode("/dev/tty4"));
        }
        else
        {
            printkf("Mounting initrd failed!\n");
        }
    }

    trigger_syscall_managePipe("pipe1", 1, 8);


    enableScheduler();

    enableInterrupts();


    while(TRUE)
    {
        //printUsageInfo();

        Tty* tty = getActiveTTY();
        if (tty && tty->update)
        {
            tty->update(tty);
        }

        halt();
    }

    return 0;
}
